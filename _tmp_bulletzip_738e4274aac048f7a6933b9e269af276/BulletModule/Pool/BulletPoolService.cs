using TEngine;
using UnityEngine;

namespace GameLogic
{
    internal sealed class BulletPoolService
    {
        private const string PoolName = "BulletViewPool";

        private readonly IObjectPool<BulletViewObject> _viewPool;
        private readonly GameObject _poolRoot;

        public BulletPoolService()
        {
            IObjectPoolModule objectPoolModule = ModuleSystem.GetModule<IObjectPoolModule>();
            _viewPool = objectPoolModule.HasObjectPool<BulletViewObject>(PoolName)
                ? objectPoolModule.GetObjectPool<BulletViewObject>(PoolName)
                : objectPoolModule.CreateMultiSpawnObjectPool<BulletViewObject>(PoolName, 32);

            _poolRoot = new GameObject("[BulletViewPoolRoot]");
            _poolRoot.transform.SetParent(GameModule.Entity.EntityRoot.transform, false);
            Object.DontDestroyOnLoad(_poolRoot);
            _poolRoot.SetActive(false);
        }

        public void AttachView(BulletEntity bullet)
        {
            BulletRuntimeData runtime = bullet?.Runtime;
            if (runtime == null || runtime.Config == null || string.IsNullOrWhiteSpace(runtime.Config.ViewAssetPath))
            {
                return;
            }

            string poolKey = string.IsNullOrWhiteSpace(runtime.Config.ViewPoolName)
                ? runtime.Config.ViewAssetPath
                : runtime.Config.ViewPoolName;

            GameObject viewInstance = null;
            BulletViewObject pooledObject = _viewPool.CanSpawn(poolKey) ? _viewPool.Spawn(poolKey) : null;
            if (pooledObject != null)
            {
                viewInstance = pooledObject.Target as GameObject;
            }
            else
            {
                try
                {
                    viewInstance = GameModule.Resource.LoadGameObject(runtime.Config.ViewAssetPath, bullet.Transform);
                }
                catch (GameFrameworkException e)
                {
                    Log.Error($"Bullet view load failed: {runtime.Config.ViewAssetPath}, error: {e.Message}");
                    return;
                }

                if (viewInstance == null)
                {
                    return;
                }

                _viewPool.Register(BulletViewObject.Create(poolKey, viewInstance), true);
            }

            runtime.ViewPoolKey = poolKey;
            runtime.ViewInstance = viewInstance;

            viewInstance.transform.SetParent(bullet.Transform, false);
            viewInstance.transform.localPosition = Vector3.zero;
            viewInstance.transform.localRotation = Quaternion.identity;
            viewInstance.SetActive(true);
            runtime.ViewBinder?.Bind(bullet, viewInstance);
        }

        public void ReleaseView(BulletEntity bullet)
        {
            BulletRuntimeData runtime = bullet?.Runtime;
            if (runtime?.ViewInstance == null)
            {
                return;
            }

            GameObject viewInstance = runtime.ViewInstance;
            runtime.ViewBinder?.Unbind(bullet, viewInstance);
            viewInstance.transform.SetParent(_poolRoot.transform, false);
            _viewPool.Unspawn(viewInstance);
            runtime.ViewInstance = null;
            runtime.ViewPoolKey = null;
        }

        public void ReleaseAllUnused()
        {
            _viewPool?.ReleaseAllUnused();
            if (_poolRoot != null)
            {
                Object.Destroy(_poolRoot);
            }
        }
    }
}
