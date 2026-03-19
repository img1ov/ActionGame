using System;
using UnityEngine;

namespace GameLogic
{
    public sealed class BulletSpawnParams
    {
        public string ConfigId;
        public BulletConfig Config;
        public long OwnerId;
        public Transform OwnerTransform;
        public Vector3 Position;
        public Vector3 Direction = Vector3.forward;
        public float SpeedOverride = -1f;
        public float LifetimeOverride = -1f;
        public Predicate<Collider> HitFilter;
        public Action<BulletHitInfo> OnHit;
        public IBulletCollisionResolver CollisionResolver;
        public IBulletViewBinder ViewBinder;
    }
}
