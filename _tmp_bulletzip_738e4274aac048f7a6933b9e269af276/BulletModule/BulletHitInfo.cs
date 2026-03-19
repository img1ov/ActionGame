using UnityEngine;

namespace GameLogic
{
    public sealed class BulletHitInfo
    {
        public BulletEntity Bullet { get; internal set; }
        public BulletConfig Config { get; internal set; }
        public long BulletId { get; internal set; }
        public long OwnerId { get; internal set; }
        public Collider HitCollider { get; internal set; }
        public Vector3 Point { get; internal set; }
        public Vector3 Normal { get; internal set; }
        public float Distance { get; internal set; }

        public GameObject HitGameObject => HitCollider != null ? HitCollider.gameObject : null;
        public Transform HitTransform => HitCollider != null ? HitCollider.transform : null;
    }
}
