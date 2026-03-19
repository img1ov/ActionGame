using System.Collections.Generic;

namespace GameLogic
{
    internal sealed class BulletActionRunner
    {
        private enum RunnerState
        {
            Idle,
            Running,
        }

        private readonly BulletActionFactory _factory = new();
        private readonly List<BulletEntity> _workBullets = new();
        private readonly List<BulletEntity> _nextWorkBullets = new();
        private RunnerState _state;
        private BulletEntity _currentBullet;

        public void Run(BulletModule module, IReadOnlyList<BulletEntity> bullets, float deltaTime, bool afterTick)
        {
            if (_state != RunnerState.Idle)
            {
                return;
            }

            _state = RunnerState.Running;
            _workBullets.Clear();
            for (int i = 0; i < bullets.Count; i++)
            {
                _workBullets.Add(bullets[i]);
            }

            RunCurrentList(module, deltaTime, afterTick);
            _workBullets.Clear();

            while (_nextWorkBullets.Count > 0)
            {
                _workBullets.AddRange(_nextWorkBullets);
                _nextWorkBullets.Clear();
                RunCurrentList(module, 0f, false);
                _workBullets.Clear();
            }

            _currentBullet = null;
            _state = RunnerState.Idle;
        }

        public void AddAction(BulletEntity bullet, BulletActionInfo actionInfo)
        {
            if (bullet?.Runtime == null || actionInfo == null)
            {
                return;
            }

            switch (_state)
            {
                case RunnerState.Idle:
                    bullet.Runtime.ActionInfos.Add(actionInfo);
                    break;
                case RunnerState.Running:
                    bullet.Runtime.NextActionInfos.Add(actionInfo);
                    if (bullet != _currentBullet)
                    {
                        _nextWorkBullets.Add(bullet);
                    }
                    break;
            }
        }

        private void RunCurrentList(BulletModule module, float deltaTime, bool afterTick)
        {
            for (int i = 0; i < _workBullets.Count; i++)
            {
                BulletEntity bullet = _workBullets[i];
                if (bullet == null || bullet.IsDisposed || bullet.Runtime == null)
                {
                    continue;
                }

                _currentBullet = bullet;
                ExecutePersistentActions(bullet, deltaTime, afterTick);
                ExecuteQueuedActions(module, bullet);
            }
        }

        private void ExecutePersistentActions(BulletEntity bullet, float deltaTime, bool afterTick)
        {
            IList<BulletActionBase> persistentActions = bullet.Runtime.PersistentActions;
            if (deltaTime > 0f)
            {
                for (int i = 0; i < persistentActions.Count; i++)
                {
                    BulletActionBase action = persistentActions[i];
                    if (afterTick)
                    {
                        action.AfterTick(deltaTime);
                    }
                    else
                    {
                        action.Tick(deltaTime);
                    }
                }
            }

            for (int i = persistentActions.Count - 1; i >= 0; i--)
            {
                if (!persistentActions[i].IsFinished)
                {
                    continue;
                }

                persistentActions[i].Reset();
                persistentActions.RemoveAt(i);
            }
        }

        private void ExecuteQueuedActions(BulletModule module, BulletEntity bullet)
        {
            while (bullet.Runtime.ActionInfos.Count > 0 || bullet.Runtime.NextActionInfos.Count > 0)
            {
                for (int i = 0; i < bullet.Runtime.ActionInfos.Count; i++)
                {
                    BulletActionInfo actionInfo = bullet.Runtime.ActionInfos[i];
                    BulletActionBase action = _factory.Create(actionInfo.ActionType);
                    if (action == null)
                    {
                        continue;
                    }

                    action.Execute(bullet, module, actionInfo);
                    if (action.IsPersistent && !action.IsFinished)
                    {
                        bullet.Runtime.PersistentActions.Add(action);
                    }
                }

                bullet.Runtime.SwapActionInfos();
            }
        }
    }
}
