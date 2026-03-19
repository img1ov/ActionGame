using System.Collections.Generic;

namespace GameLogic
{
    internal sealed class BulletTimeControlSystem
    {
        public void Update(IReadOnlyList<BulletEntity> bullets, float deltaTime)
        {
            int count = bullets.Count;
            for (int i = 0; i < count; i++)
            {
                BulletEntity bullet = bullets[i];
                BulletRuntimeData runtime = bullet?.Runtime;
                if (runtime == null || bullet.IsDisposed)
                {
                    continue;
                }

                runtime.UpdateFrameDeltaTime(deltaTime);
            }
        }
    }
}
