using System.Collections.Generic;
using UnityEngine;

namespace GameLogic
{
    internal sealed class BulletCollisionSystem
    {
        private readonly RaycastHit[] _raycastHits = new RaycastHit[16];
        private readonly Collider[] _overlapHits = new Collider[16];

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

                if (runtime.EffectiveTimeScale <= 0f)
                {
                    continue;
                }

                if (!TryGetHitInfo(bullet, out BulletHitInfo hitInfo))
                {
                    continue;
                }

                runtime.RecordHit(hitInfo.HitCollider);
                runtime.CollisionResolver?.ResolveHit(hitInfo);
                runtime.OnHit?.Invoke(hitInfo);

                if (runtime.Config.DestroyOnHit || (runtime.Config.MaxHitCount > 0 && runtime.HitCount >= runtime.Config.MaxHitCount))
                {
                    module.RequestDestroy(bullet, BulletDestroyReason.HitTarget);
                }
            }
        }

        private bool TryGetHitInfo(BulletEntity bullet, out BulletHitInfo hitInfo)
        {
            BulletRuntimeData runtime = bullet.Runtime;
            Vector3 displacement = runtime.Position - runtime.PreviousPosition;
            float distance = displacement.magnitude;

            if (runtime.Config.CollisionShape == BulletCollisionShape.Ray)
            {
                float rayDistance = distance > 0.0001f ? distance : Mathf.Max(runtime.Config.RaycastLength, 0.01f);
                Vector3 direction = distance > 0.0001f ? displacement / distance : runtime.Direction;
                return TryRaycastHit(bullet, runtime.PreviousPosition, direction, rayDistance, out hitInfo);
            }

            if (distance <= 0.0001f)
            {
                return TryOverlapHit(bullet, runtime.Position, out hitInfo);
            }

            return TrySphereCastHit(bullet, runtime.PreviousPosition, displacement / distance, distance, out hitInfo);
        }

        private bool TryRaycastHit(BulletEntity bullet, Vector3 origin, Vector3 direction, float distance, out BulletHitInfo hitInfo)
        {
            int hitCount = Physics.RaycastNonAlloc(origin, direction, _raycastHits, distance, bullet.Runtime.Config.HitMask, bullet.Runtime.Config.TriggerInteraction);
            return TryGetNearestRaycastHit(bullet, hitCount, out hitInfo);
        }

        private bool TrySphereCastHit(BulletEntity bullet, Vector3 origin, Vector3 direction, float distance, out BulletHitInfo hitInfo)
        {
            int hitCount = Physics.SphereCastNonAlloc(origin, bullet.Runtime.Config.Radius, direction, _raycastHits, distance, bullet.Runtime.Config.HitMask, bullet.Runtime.Config.TriggerInteraction);
            return TryGetNearestRaycastHit(bullet, hitCount, out hitInfo);
        }

        private bool TryOverlapHit(BulletEntity bullet, Vector3 position, out BulletHitInfo hitInfo)
        {
            int hitCount = Physics.OverlapSphereNonAlloc(position, bullet.Runtime.Config.Radius, _overlapHits, bullet.Runtime.Config.HitMask, bullet.Runtime.Config.TriggerInteraction);
            float bestDistance = float.MaxValue;
            Collider bestCollider = null;
            Vector3 bestPoint = default;
            Vector3 bestNormal = default;

            for (int i = 0; i < hitCount; i++)
            {
                Collider collider = _overlapHits[i];
                if (!IsValidHit(bullet, collider))
                {
                    continue;
                }

                Vector3 point = collider.ClosestPoint(position);
                float distance = Vector3.SqrMagnitude(position - point);
                if (distance >= bestDistance)
                {
                    continue;
                }

                bestDistance = distance;
                bestCollider = collider;
                bestPoint = point;
                Vector3 normal = (position - point).normalized;
                bestNormal = normal.sqrMagnitude > 0.0001f ? normal : -bullet.Runtime.Direction;
            }

            if (bestCollider == null)
            {
                hitInfo = null;
                return false;
            }

            hitInfo = CreateHitInfo(bullet, bestCollider, bestPoint, bestNormal, Mathf.Sqrt(bestDistance));
            return true;
        }

        private bool TryGetNearestRaycastHit(BulletEntity bullet, int hitCount, out BulletHitInfo hitInfo)
        {
            float bestDistance = float.MaxValue;
            RaycastHit bestHit = default;
            bool hasValidHit = false;

            for (int i = 0; i < hitCount; i++)
            {
                RaycastHit raycastHit = _raycastHits[i];
                if (!IsValidHit(bullet, raycastHit.collider))
                {
                    continue;
                }

                if (raycastHit.distance >= bestDistance)
                {
                    continue;
                }

                bestDistance = raycastHit.distance;
                bestHit = raycastHit;
                hasValidHit = true;
            }

            if (!hasValidHit)
            {
                hitInfo = null;
                return false;
            }

            hitInfo = CreateHitInfo(bullet, bestHit.collider, bestHit.point, bestHit.normal, bestHit.distance);
            return true;
        }

        private bool IsValidHit(BulletEntity bullet, Collider collider)
        {
            BulletRuntimeData runtime = bullet.Runtime;
            if (collider == null || !collider.enabled || !collider.gameObject.activeInHierarchy)
            {
                return false;
            }

            if (bullet.Transform != null && collider.transform.IsChildOf(bullet.Transform))
            {
                return false;
            }

            if (runtime.Config.IgnoreOwner && runtime.IsOwnerCollider(collider))
            {
                return false;
            }

            if (runtime.Config.IgnoreAlreadyHit && runtime.HasHit(collider))
            {
                return false;
            }

            if (runtime.HitFilter != null && !runtime.HitFilter(collider))
            {
                return false;
            }

            return runtime.CollisionResolver == null || runtime.CollisionResolver.CanHit(bullet, collider);
        }

        private static BulletHitInfo CreateHitInfo(BulletEntity bullet, Collider collider, Vector3 point, Vector3 normal, float distance)
        {
            return new BulletHitInfo
            {
                Bullet = bullet,
                BulletId = bullet.Id,
                OwnerId = bullet.Runtime.OwnerId,
                Config = bullet.Runtime.Config,
                HitCollider = collider,
                Point = point,
                Normal = normal,
                Distance = distance,
            };
        }
    }
}
