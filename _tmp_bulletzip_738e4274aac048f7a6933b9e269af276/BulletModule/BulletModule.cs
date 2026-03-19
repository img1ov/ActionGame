using System.Collections.Generic;
using TEngine;
using UnityEngine;

namespace GameLogic
{
    public sealed class BulletModule : Singleton<BulletModule>, IBulletModule, IUpdate
    {
        private const string BulletSceneName = "BulletScene";

        private readonly Dictionary<long, BulletEntity> _bullets = new();
        private readonly Dictionary<long, HashSet<BulletEntity>> _ownerBullets = new();
        private readonly Dictionary<long, float> _ownerInheritedTimeScales = new();
        private readonly List<BulletEntity> _updateBuffer = new();

        private BulletConfigProvider _configProvider;
        private BulletPoolService _poolService;
        private BulletLifecycleSystem _lifecycleSystem;
        private BulletTimeControlSystem _timeControlSystem;
        private BulletMoveSystem _moveSystem;
        private BulletCollisionSystem _collisionSystem;
        private BulletActionRunner _actionRunner;
        private BulletScene _scene;

        internal BulletLifecycleSystem LifecycleSystem => _lifecycleSystem;

        protected override void OnInit()
        {
            _configProvider = new BulletConfigProvider();
            _poolService = new BulletPoolService();
            _lifecycleSystem = new BulletLifecycleSystem();
            _timeControlSystem = new BulletTimeControlSystem();
            _moveSystem = new BulletMoveSystem();
            _collisionSystem = new BulletCollisionSystem();
            _actionRunner = new BulletActionRunner();
            EnsureScene();
        }

        public BulletEntity Spawn(BulletSpawnParams spawnParams)
        {
            if (spawnParams == null)
            {
                Log.Error("Bullet spawn params is null.");
                return null;
            }

            BulletConfig config = ResolveConfig(spawnParams);
            if (config == null || !config.IsValid())
            {
                Log.Error($"Bullet config not found. ConfigId: {spawnParams.ConfigId}");
                return null;
            }

            EnsureScene();

            BulletRuntimeData runtime = new BulletRuntimeData();
            runtime.Initialize(config, spawnParams);

            BulletEntity bullet = _scene.AddChild<BulletEntity, BulletRuntimeData>(runtime);
            if (bullet == null)
            {
                Log.Error($"Bullet entity create failed. ConfigId: {config.Id}");
                return null;
            }

            _bullets[bullet.Id] = bullet;
            AddOwnerIndex(runtime.OwnerId, bullet);
            ApplyOwnerInheritedTimeScale(runtime);
            _poolService.AttachView(bullet);
            AddAction(bullet, BulletActionInfo.Create(BulletActionType.Init));
            return bullet;
        }

        public bool Destroy(long bulletId, BulletDestroyReason reason = BulletDestroyReason.Manual)
        {
            return TryGet(bulletId, out BulletEntity bullet) && RequestDestroy(bullet, reason);
        }

        public void DestroyByOwner(long ownerId, BulletDestroyReason reason = BulletDestroyReason.Manual)
        {
            ForEachOwnerBullet(ownerId, bullet => RequestDestroy(bullet, reason));
        }

        public bool Freeze(long bulletId, float durationSeconds = 0f)
        {
            if (!TryGet(bulletId, out BulletEntity bullet))
            {
                return false;
            }

            BulletRuntimeData runtime = bullet.Runtime;
            if (runtime == null || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                return false;
            }

            runtime.Freeze(durationSeconds);
            return true;
        }

        public void FreezeByOwner(long ownerId, float durationSeconds = 0f)
        {
            ForEachOwnerBullet(ownerId, bullet => bullet.Runtime?.Freeze(durationSeconds));
        }

        public bool Unfreeze(long bulletId)
        {
            if (!TryGet(bulletId, out BulletEntity bullet))
            {
                return false;
            }

            BulletRuntimeData runtime = bullet.Runtime;
            if (runtime == null || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                return false;
            }

            runtime.Unfreeze();
            return true;
        }

        public void UnfreezeByOwner(long ownerId)
        {
            ForEachOwnerBullet(ownerId, bullet => bullet.Runtime?.Unfreeze());
        }

        public bool SetBulletTimeScale(long bulletId, float timeScale)
        {
            if (!TryGet(bulletId, out BulletEntity bullet))
            {
                return false;
            }

            BulletRuntimeData runtime = bullet.Runtime;
            if (runtime == null || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                return false;
            }

            runtime.SetBulletTimeScale(timeScale);
            return true;
        }

        public void SetBulletTimeScaleByOwner(long ownerId, float timeScale)
        {
            ForEachOwnerBullet(ownerId, bullet => bullet.Runtime?.SetBulletTimeScale(timeScale));
        }

        public bool SetInheritedTimeScale(long bulletId, float timeScale)
        {
            if (!TryGet(bulletId, out BulletEntity bullet))
            {
                return false;
            }

            BulletRuntimeData runtime = bullet.Runtime;
            if (runtime == null || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                return false;
            }

            runtime.SetInheritedTimeScale(timeScale);
            return true;
        }

        public void SetInheritedTimeScaleByOwner(long ownerId, float timeScale)
        {
            if (ownerId == 0)
            {
                return;
            }

            float clampedTimeScale = Mathf.Max(0f, timeScale);
            if (Mathf.Approximately(clampedTimeScale, 1f))
            {
                _ownerInheritedTimeScales.Remove(ownerId);
            }
            else
            {
                _ownerInheritedTimeScales[ownerId] = clampedTimeScale;
            }

            ForEachOwnerBullet(ownerId, bullet => bullet.Runtime?.SetInheritedTimeScale(timeScale));
        }

        public BulletEntity Get(long bulletId)
        {
            TryGet(bulletId, out BulletEntity bullet);
            return bullet;
        }

        public bool TryGet(long bulletId, out BulletEntity bullet)
        {
            return _bullets.TryGetValue(bulletId, out bullet) && bullet != null && !bullet.IsDisposed;
        }

        public bool RegisterConfig(BulletConfig config)
        {
            bool result = _configProvider.Register(config);
            if (!result)
            {
                Log.Error($"Register bullet config failed. ConfigId: {config?.Id}");
            }

            return result;
        }

        public bool TryGetConfig(string configId, out BulletConfig config)
        {
            return _configProvider.TryGet(configId, out config);
        }

        public void Clear()
        {
            if (_bullets.Count == 0)
            {
                return;
            }

            List<BulletEntity> bulletSnapshot = new List<BulletEntity>(_bullets.Values);
            for (int i = 0; i < bulletSnapshot.Count; i++)
            {
                ForceDestroy(bulletSnapshot[i], BulletDestroyReason.ModuleClear);
            }
            _lifecycleSystem.Flush(this);
        }

        public void OnUpdate()
        {
            if (_bullets.Count == 0)
            {
                return;
            }

            float deltaTime = GameTimer.IsValid ? GameTimer.Instance.DeltaTime : Time.deltaTime;
            if (deltaTime <= 0f)
            {
                return;
            }

            RebuildUpdateBuffer();
            _timeControlSystem.Update(_updateBuffer, deltaTime);
            _actionRunner.Run(this, _updateBuffer, deltaTime, false);
            _moveSystem.Update(_updateBuffer, this);
            _collisionSystem.Update(_updateBuffer, this);
            RebuildUpdateBuffer();
            _actionRunner.Run(this, _updateBuffer, deltaTime, true);
            _updateBuffer.Clear();
            _lifecycleSystem.Flush(this);
        }

        public void AddAction(BulletEntity bullet, BulletActionInfo actionInfo)
        {
            _actionRunner.AddAction(bullet, actionInfo);
        }

        public void DelayDestroy(BulletEntity bullet, float delaySeconds, BulletDestroyReason reason = BulletDestroyReason.Manual, bool ignoreBulletTimeScale = false)
        {
            if (bullet?.Runtime == null || bullet.IsDisposed || bullet.Runtime.DestroyRequested || bullet.Runtime.PendingDestroy)
            {
                return;
            }

            AddAction(bullet, new BulletActionInfo
            {
                ActionType = BulletActionType.DelayDestroy,
                DelaySeconds = delaySeconds,
                IgnoreBulletTimeScale = ignoreBulletTimeScale,
                DestroyReason = reason,
            });
        }

        public void AddCallbackAction(BulletEntity bullet, System.Action<BulletEntity, BulletModule> callback)
        {
            if (bullet == null || callback == null)
            {
                return;
            }

            AddAction(bullet, new BulletActionInfo
            {
                ActionType = BulletActionType.Callback,
                Callback = callback,
            });
        }

        internal bool RequestDestroy(BulletEntity bullet, BulletDestroyReason reason)
        {
            BulletRuntimeData runtime = bullet?.Runtime;
            if (runtime == null || bullet.IsDisposed || runtime.DestroyRequested || runtime.PendingDestroy)
            {
                return false;
            }

            runtime.DestroyRequested = true;
            runtime.DestroyReason = reason;

            BulletActionInfo destroyAction = BulletActionInfo.Create(BulletActionType.Destroy);
            destroyAction.DestroyReason = reason;
            AddAction(bullet, destroyAction);
            return true;
        }

        internal void DestroyImmediately(long bulletId)
        {
            if (!_bullets.TryGetValue(bulletId, out BulletEntity bullet) || bullet == null)
            {
                return;
            }

            BulletRuntimeData runtime = bullet.Runtime;
            if (runtime != null)
            {
                RemoveOwnerIndex(runtime.OwnerId, bullet);
                _poolService.ReleaseView(bullet);
            }

            _bullets.Remove(bulletId);
            bullet.Dispose();
        }

        protected override void OnRelease()
        {
            Clear();
            _lifecycleSystem.Clear();
            _configProvider.Clear();

            if (_scene != null)
            {
                GameModule.Entity.RemoveScene(BulletSceneName);
                _scene = null;
            }

            _ownerInheritedTimeScales.Clear();
            _ownerBullets.Clear();
            _bullets.Clear();
            _updateBuffer.Clear();
            _poolService.ReleaseAllUnused();
            _poolService = null;
            _collisionSystem = null;
            _moveSystem = null;
            _timeControlSystem = null;
            _lifecycleSystem = null;
            _actionRunner = null;
            _configProvider = null;
        }

        // Teardown skips gameplay destroy actions and only guarantees cleanup.
        private void ForceDestroy(BulletEntity bullet, BulletDestroyReason reason)
        {
            if (bullet?.Runtime == null || bullet.IsDisposed)
            {
                return;
            }

            _lifecycleSystem.MarkDestroy(bullet, reason);
        }

        private void EnsureScene()
        {
            if (_scene != null && !_scene.IsDisposed)
            {
                return;
            }

            _scene = GameModule.Entity.GetScene(BulletSceneName) as BulletScene;
            if (_scene == null)
            {
                _scene = new BulletScene(BulletSceneName);
                GameModule.Entity.AddScene(BulletSceneName, _scene);
            }
        }

        private BulletConfig ResolveConfig(BulletSpawnParams spawnParams)
        {
            if (spawnParams.Config != null)
            {
                return spawnParams.Config;
            }

            _configProvider.TryGet(spawnParams.ConfigId, out BulletConfig config);
            return config;
        }

        private void RebuildUpdateBuffer()
        {
            _updateBuffer.Clear();
            foreach (BulletEntity bullet in _bullets.Values)
            {
                _updateBuffer.Add(bullet);
            }
        }

        private void ApplyOwnerInheritedTimeScale(BulletRuntimeData runtime)
        {
            if (runtime == null || runtime.OwnerId == 0)
            {
                return;
            }

            if (_ownerInheritedTimeScales.TryGetValue(runtime.OwnerId, out float timeScale))
            {
                runtime.SetInheritedTimeScale(timeScale);
            }
        }

        private void ForEachOwnerBullet(long ownerId, System.Action<BulletEntity> visitor)
        {
            if (ownerId == 0 || visitor == null)
            {
                return;
            }

            if (!_ownerBullets.TryGetValue(ownerId, out HashSet<BulletEntity> bullets) || bullets.Count == 0)
            {
                return;
            }

            List<BulletEntity> bulletSnapshot = new List<BulletEntity>(bullets);
            for (int i = 0; i < bulletSnapshot.Count; i++)
            {
                BulletEntity bullet = bulletSnapshot[i];
                if (bullet == null || bullet.IsDisposed || bullet.Runtime == null)
                {
                    continue;
                }

                visitor(bullet);
            }
        }

        private void AddOwnerIndex(long ownerId, BulletEntity bullet)
        {
            if (ownerId == 0)
            {
                return;
            }

            if (!_ownerBullets.TryGetValue(ownerId, out HashSet<BulletEntity> bulletSet))
            {
                bulletSet = new HashSet<BulletEntity>();
                _ownerBullets.Add(ownerId, bulletSet);
            }

            bulletSet.Add(bullet);
        }

        private void RemoveOwnerIndex(long ownerId, BulletEntity bullet)
        {
            if (ownerId == 0 || !_ownerBullets.TryGetValue(ownerId, out HashSet<BulletEntity> bulletSet))
            {
                return;
            }

            bulletSet.Remove(bullet);
            if (bulletSet.Count == 0)
            {
                _ownerBullets.Remove(ownerId);
            }
        }
    }
}
