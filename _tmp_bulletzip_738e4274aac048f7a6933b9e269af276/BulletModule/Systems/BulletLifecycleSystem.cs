using System.Collections.Generic;

namespace GameLogic
{
    internal sealed class BulletLifecycleSystem
    {
        // Tracks bullets that should be destroyed in the next flush.
        private readonly HashSet<long> _pendingDestroyIds = new();

        public bool MarkDestroy(BulletEntity bullet, BulletDestroyReason reason)
        {
            if (bullet?.Runtime == null || bullet.IsDisposed || bullet.Runtime.PendingDestroy)
            {
                return false;
            }

            bullet.Runtime.DestroyRequested = true;
            bullet.Runtime.PendingDestroy = true;
            bullet.Runtime.DestroyReason = reason;
            _pendingDestroyIds.Add(bullet.Id);
            return true;
        }

        public void Flush(BulletModule module)
        {
            if (_pendingDestroyIds.Count == 0)
            {
                return;
            }

            // Destroy marked bullets in a single pass to keep lifecycle updates ordered.
            foreach (long bulletId in _pendingDestroyIds)
            {
                module.DestroyImmediately(bulletId);
            }

            _pendingDestroyIds.Clear();
        }

        public void Clear()
        {
            _pendingDestroyIds.Clear();
        }
    }
}
