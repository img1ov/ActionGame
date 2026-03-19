using System.Collections.Generic;
using TEngine;
using UnityEngine;

namespace GameLogic
{
    public interface IEntityModule
    {
        GameObject EntityRoot { get; }
        EntityScene GetScene(string sceneName);
        EntityScene AddScene(string sceneName, EntityScene scene);
        void RemoveScene(string sceneName);
    }

    public class EntityModule : Singleton<EntityModule>, IUpdate, IFixedUpdate, ILateUpdate
    {
        public GameObject EntityRoot { get; private set; }

        private readonly Dictionary<string, EntityScene> _scenes = new();

        protected override void OnInit()
        {
            EntityRoot = new GameObject("[EntityRoot]");
            Object.DontDestroyOnLoad(EntityRoot);

            var _ = EntityIdGenerator.Instance;
            var __ = ComponentLifecycleManager.Instance;
            var ___ = EntityDependencyRegistry.Instance;
            var ____ = GameTimer.Instance;
        }

        public EntityScene GetScene(string sceneName)
        {
            return _scenes.TryGetValue(sceneName, out var scene) ? scene : null;
        }

        public EntityScene AddScene(string sceneName, EntityScene scene)
        {
            if (_scenes.ContainsKey(sceneName))
                throw new System.Exception($"scene {sceneName} already exists");
            _scenes.Add(sceneName, scene);
            scene.Awake();
            return scene;
        }

        public void RemoveScene(string sceneName)
        {
            if (_scenes.TryGetValue(sceneName, out var scene))
            {
                scene.Dispose();
                _scenes.Remove(sceneName);
            }
        }

        public void OnUpdate()
        {
            GameTimer.Instance?.Update(Time.deltaTime, Time.unscaledDeltaTime);
        }

        public void OnFixedUpdate()
        {
            GameTimer.Instance?.FixedUpdate(Time.fixedDeltaTime);
        }

        public void OnLateUpdate()
        {
            GameTimer.Instance?.LateUpdate();
        }

        protected override void OnRelease()
        {
            foreach (var kv in _scenes)
                kv.Value.Dispose();
            _scenes.Clear();

            GameTimer.Instance?.Release();
            EntityDependencyRegistry.Instance?.Release();
            ComponentLifecycleManager.Instance?.Release();
            EntityIdGenerator.Instance?.Release();

            if (EntityRoot != null)
            {
                Object.Destroy(EntityRoot);
                EntityRoot = null;
            }
        }
    }
}
