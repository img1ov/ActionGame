using TEngine;
using UnityEngine;
using Object = UnityEngine.Object;

namespace GameLogic
{
    internal sealed class BulletViewObject : ObjectBase
    {
        public static BulletViewObject Create(string name, GameObject target)
        {
            BulletViewObject item = MemoryPool.Acquire<BulletViewObject>();
            item.Initialize(name, target);
            return item;
        }

        protected override void OnSpawn()
        {
            base.OnSpawn();
            if (Target is GameObject gameObject)
            {
                gameObject.SetActive(true);
            }
        }

        protected override void OnUnspawn()
        {
            base.OnUnspawn();
            if (Target is GameObject gameObject)
            {
                gameObject.SetActive(false);
            }
        }

        protected override void Release(bool isShutdown)
        {
            if (Target is GameObject gameObject)
            {
                Object.Destroy(gameObject);
            }
        }
    }
}
