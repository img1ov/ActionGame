using UnityEngine;

namespace GameLogic
{
    public interface IBulletCollisionResolver
    {
        bool CanHit(BulletEntity bullet, Collider collider);
        void ResolveHit(BulletHitInfo hitInfo);
    }
}
