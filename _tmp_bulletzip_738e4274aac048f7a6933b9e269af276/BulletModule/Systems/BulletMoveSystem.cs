using System.Collections.Generic;
using UnityEngine;

namespace GameLogic
{
    internal sealed class BulletMoveSystem
    {
        public void Update(IReadOnlyList<BulletEntity> bullets, BulletModule module)
        {
            int count = bullets.Count;
            for (int i = 0; i < count; i++)
            {
                BulletEntity bullet = bullets[i];
                BulletRuntimeData runtime = bullet?.Runtime;
                if (runtime == null || runtime.DestroyRequested || runtime.PendingDestroy || bullet.IsDisposed)
                {
                    continue;
                }

                if (runtime.OwnerTransform == null && runtime.OwnerId != 0)
                {
                    module.RequestDestroy(bullet, BulletDestroyReason.OwnerMissing);
                    continue;
                }

                float deltaTime = runtime.ScaledDeltaTime;
                if (deltaTime <= 0f)
                {
                    continue;
                }

                runtime.PreviousPosition = runtime.Position;
                runtime.Position += runtime.Velocity * deltaTime;

                if (runtime.Config.AlignToVelocity && runtime.Velocity.sqrMagnitude > 0.0001f)
                {
                    runtime.Direction = runtime.Velocity.normalized;
                }

                bullet.SyncTransform(runtime.Position, runtime.Direction);
            }
        }
    }
}
