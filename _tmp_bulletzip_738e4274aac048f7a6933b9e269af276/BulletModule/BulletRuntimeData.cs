using System;
using System.Collections.Generic;
using UnityEngine;

namespace GameLogic
{
    public sealed class BulletRuntimeData
    {
        private readonly HashSet<int> _ownerColliderIds = new();
        private readonly HashSet<int> _hitColliderIds = new();
        private readonly List<BulletActionInfo> _actionInfos = new();
        private readonly List<BulletActionInfo> _nextActionInfos = new();
        private readonly List<BulletActionBase> _persistentActions = new();

        public long BulletId;
        public long OwnerId;
        public Transform OwnerTransform;
        public BulletConfig Config;
        public Vector3 PreviousPosition;
        public Vector3 Position;
        public Vector3 Direction;
        public Vector3 Velocity;
        public float Speed;
        public float Lifetime;
        public float MaxLifetime;
        public bool IsFrozen;
        public bool HasFrozenTimer;
        public float FrozenRemainingTime;
        public float BulletTimeScale;
        public float InheritedTimeScale;
        public float EffectiveTimeScale;
        public float ModuleDeltaTime;
        public float ScaledDeltaTime;
        public int HitCount;
        public bool IsInitialized;
        public bool DestroyRequested;
        public bool PendingDestroy;
        public BulletDestroyReason DestroyReason;
        public GameObject ViewInstance;
        public string ViewPoolKey;
        public Predicate<Collider> HitFilter;
        public Action<BulletHitInfo> OnHit;
        public IBulletCollisionResolver CollisionResolver;
        public IBulletViewBinder ViewBinder;

        public IList<BulletActionInfo> ActionInfos => _actionInfos;
        public IList<BulletActionInfo> NextActionInfos => _nextActionInfos;
        public IList<BulletActionBase> PersistentActions => _persistentActions;

        public void Initialize(BulletConfig config, BulletSpawnParams spawnParams)
        {
            Config = config;
            OwnerTransform = spawnParams.OwnerTransform;
            OwnerId = spawnParams.OwnerId != 0
                ? spawnParams.OwnerId
                : spawnParams.OwnerTransform != null
                    ? spawnParams.OwnerTransform.GetInstanceID()
                    : 0;
            Position = spawnParams.Position;
            PreviousPosition = spawnParams.Position;
            Direction = spawnParams.Direction.sqrMagnitude > 0.0001f
                ? spawnParams.Direction.normalized
                : spawnParams.OwnerTransform != null
                    ? spawnParams.OwnerTransform.forward
                    : Vector3.forward;
            Speed = spawnParams.SpeedOverride > 0f ? spawnParams.SpeedOverride : config.Speed;
            Lifetime = 0f;
            MaxLifetime = spawnParams.LifetimeOverride > 0f ? spawnParams.LifetimeOverride : config.MaxLifetime;
            Velocity = Direction * Speed;
            IsFrozen = false;
            HasFrozenTimer = false;
            FrozenRemainingTime = 0f;
            BulletTimeScale = 1f;
            InheritedTimeScale = 1f;
            EffectiveTimeScale = 1f;
            ModuleDeltaTime = 0f;
            ScaledDeltaTime = 0f;
            DestroyRequested = false;
            PendingDestroy = false;
            DestroyReason = BulletDestroyReason.None;
            HitFilter = spawnParams.HitFilter;
            OnHit = spawnParams.OnHit;
            CollisionResolver = spawnParams.CollisionResolver;
            ViewBinder = spawnParams.ViewBinder;

            CacheOwnerColliders();
        }

        public bool IsOwnerCollider(Collider collider)
        {
            return collider != null && _ownerColliderIds.Contains(collider.GetInstanceID());
        }

        public bool HasHit(Collider collider)
        {
            return collider != null && _hitColliderIds.Contains(collider.GetInstanceID());
        }

        public void RecordHit(Collider collider)
        {
            if (collider == null)
            {
                return;
            }

            _hitColliderIds.Add(collider.GetInstanceID());
            HitCount++;
        }

        public void Reset()
        {
            BulletId = 0;
            OwnerId = 0;
            OwnerTransform = null;
            Config = null;
            PreviousPosition = default;
            Position = default;
            Direction = Vector3.forward;
            Velocity = default;
            Speed = 0f;
            Lifetime = 0f;
            MaxLifetime = 0f;
            IsFrozen = false;
            HasFrozenTimer = false;
            FrozenRemainingTime = 0f;
            BulletTimeScale = 1f;
            InheritedTimeScale = 1f;
            EffectiveTimeScale = 1f;
            ModuleDeltaTime = 0f;
            ScaledDeltaTime = 0f;
            HitCount = 0;
            IsInitialized = false;
            DestroyRequested = false;
            PendingDestroy = false;
            DestroyReason = BulletDestroyReason.None;
            ViewInstance = null;
            ViewPoolKey = null;
            HitFilter = null;
            OnHit = null;
            CollisionResolver = null;
            ViewBinder = null;
            _ownerColliderIds.Clear();
            _hitColliderIds.Clear();
            _actionInfos.Clear();
            _nextActionInfos.Clear();
            _persistentActions.Clear();
        }

        public void SwapActionInfos()
        {
            _actionInfos.Clear();
            if (_nextActionInfos.Count <= 0)
            {
                return;
            }

            _actionInfos.AddRange(_nextActionInfos);
            _nextActionInfos.Clear();
        }

        public void Freeze(float durationSeconds)
        {
            IsFrozen = true;
            HasFrozenTimer = durationSeconds > 0f;
            FrozenRemainingTime = HasFrozenTimer ? durationSeconds : 0f;
            RefreshDeltaTime();
        }

        public void Unfreeze()
        {
            IsFrozen = false;
            HasFrozenTimer = false;
            FrozenRemainingTime = 0f;
            RefreshDeltaTime();
        }

        public void SetBulletTimeScale(float timeScale)
        {
            BulletTimeScale = Mathf.Max(0f, timeScale);
            RefreshDeltaTime();
        }

        public void SetInheritedTimeScale(float timeScale)
        {
            InheritedTimeScale = Mathf.Max(0f, timeScale);
            RefreshDeltaTime();
        }

        public void UpdateFrameDeltaTime(float deltaTime)
        {
            ModuleDeltaTime = Mathf.Max(0f, deltaTime);

            if (IsFrozen && HasFrozenTimer)
            {
                FrozenRemainingTime -= ModuleDeltaTime;
                if (FrozenRemainingTime <= 0f)
                {
                    Unfreeze();
                    return;
                }
            }

            RefreshDeltaTime();
        }

        public float GetActionDeltaTime(bool ignoreBulletTimeScale)
        {
            if (!ignoreBulletTimeScale)
            {
                return ScaledDeltaTime;
            }

            return ModuleDeltaTime * Mathf.Max(0f, InheritedTimeScale);
        }

        private void RefreshDeltaTime()
        {
            float bulletTimeScale = IsFrozen ? 0f : BulletTimeScale;
            EffectiveTimeScale = Mathf.Max(0f, bulletTimeScale * InheritedTimeScale);
            ScaledDeltaTime = ModuleDeltaTime * EffectiveTimeScale;
        }

        private void CacheOwnerColliders()
        {
            _ownerColliderIds.Clear();

            if (OwnerTransform == null)
            {
                return;
            }

            Collider[] colliders = OwnerTransform.GetComponentsInChildren<Collider>(true);
            for (int i = 0; i < colliders.Length; i++)
            {
                Collider collider = colliders[i];
                if (collider != null)
                {
                    _ownerColliderIds.Add(collider.GetInstanceID());
                }
            }
        }
    }
}
