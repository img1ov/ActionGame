using System;
using UnityEngine;

namespace GameLogic
{
    [Serializable]
    public sealed class BulletConfig
    {
        public string Id;
        public string ViewAssetPath;
        public string ViewPoolName;
        public BulletCollisionShape CollisionShape = BulletCollisionShape.Sphere;
        public float Speed = 20f;
        public float MaxLifetime = 3f;
        public float Radius = 0.2f;
        public float RaycastLength = 0.25f;
        public bool AlignToVelocity = true;
        public bool DestroyOnHit = true;
        public bool IgnoreOwner = true;
        public bool IgnoreAlreadyHit = true;
        public int MaxHitCount = 1;
        public LayerMask HitMask = ~0;
        public QueryTriggerInteraction TriggerInteraction = QueryTriggerInteraction.Ignore;

        public bool IsValid()
        {
            return !string.IsNullOrWhiteSpace(Id) && MaxLifetime > 0f && Radius >= 0f;
        }
    }
}
