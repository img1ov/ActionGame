using UnityEngine;

namespace GameLogic
{
    public sealed class BulletEntity : Entity, IEntityAwake<BulletRuntimeData>, IEntityDestroy
    {
        public BulletRuntimeData Runtime { get; private set; }

        public Transform Transform => ViewGO != null ? ViewGO.transform : null;

        public void Awake(BulletRuntimeData runtime)
        {
            Runtime = runtime;
            Runtime.BulletId = Id;

            if (ViewGO != null)
            {
                ViewGO.name = $"BulletEntity_{Id}_{Runtime.Config?.Id}";
            }

            SyncTransform(Runtime.Position, Runtime.Direction);
            Runtime.IsInitialized = true;
        }

        public void SyncTransform(Vector3 position, Vector3 direction)
        {
            if (Transform == null)
            {
                return;
            }

            Transform.position = position;
            if (direction.sqrMagnitude > 0.0001f)
            {
                Transform.rotation = Quaternion.LookRotation(direction.normalized);
            }
        }

        public void OnDestroy()
        {
            Runtime?.Reset();
            Runtime = null;
        }
    }
}
