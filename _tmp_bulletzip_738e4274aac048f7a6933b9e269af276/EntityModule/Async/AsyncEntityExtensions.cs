using System;
using System.Threading;
using Cysharp.Threading.Tasks;
using TEngine;

namespace GameLogic
{
    public static class AsyncEntityExtensions
    {
        private static async UniTask AsyncEntityInitialize<T>(Entity entity, AsyncEntity component, CancellationToken cancelToken)
        {
            Type type = typeof(T);
            try { await component.InitializeAsync(cancelToken); }
            catch (OperationCanceledException)
            {
                Log.Warning($"AsyncEntity load cancelled, removing component: {type.FullName}");
                entity.RemoveComponent(type);
                throw;
            }
            catch (Exception ex)
            {
                Log.Error($"AsyncEntity load error: {ex}  removing component: {type.FullName}");
                entity.RemoveComponent(type);
                throw;
            }
        }

        public static async UniTask<T> AddComponentWithIdAsync<T>(this Entity entity, long id, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            Type type = typeof(T);
            if (entity._components != null && entity._components.ContainsKey(entity.GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            var component = entity.AddComponent(type, isFromPool) as AsyncEntity;
            component.Id = id;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded)
            {
                component.ProcessComponentDependencies();
                lifecycle.RegisterComponent(component);
                return component as T;
            }
            return null;
        }

        public static async UniTask<T> AddComponentWithIdAsync<T, P1>(this Entity entity, long id, P1 p1, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1>, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            Type type = typeof(T);
            if (entity._components != null && entity._components.ContainsKey(entity.GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            var component = entity.AddComponent(type, isFromPool) as AsyncEntity;
            component.Id = id;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded)
            {
                EntityDependencyRegistry.Instance?.NotifyAddComponent(entity, typeof(T));
                component.ProcessComponentDependencies();
                lifecycle.RegisterComponent(component);
                return component as T;
            }
            return null;
        }

        public static async UniTask<T> AddComponentWithIdAsync<T, P1, P2>(this Entity entity, long id, P1 p1, P2 p2, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1, P2>, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            Type type = typeof(T);
            if (entity._components != null && entity._components.ContainsKey(entity.GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            var component = entity.AddComponent(type, isFromPool) as AsyncEntity;
            component.Id = id;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1, p2);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded)
            {
                EntityDependencyRegistry.Instance?.NotifyAddComponent(entity, typeof(T));
                component.ProcessComponentDependencies();
                lifecycle.RegisterComponent(component);
                return component as T;
            }
            return null;
        }

        public static async UniTask<T> AddComponentWithIdAsync<T, P1, P2, P3>(this Entity entity, long id, P1 p1, P2 p2, P3 p3, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1, P2, P3>, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            Type type = typeof(T);
            if (entity._components != null && entity._components.ContainsKey(entity.GetLongHashCode(type)))
                throw new Exception($"entity already has component: {type.FullName}");
            var component = entity.AddComponent(type, isFromPool) as AsyncEntity;
            component.Id = id;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1, p2, p3);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded)
            {
                EntityDependencyRegistry.Instance?.NotifyAddComponent(entity, typeof(T));
                component.ProcessComponentDependencies();
                lifecycle.RegisterComponent(component);
                return component as T;
            }
            return null;
        }

        public static async UniTask<T> AddComponentAsync<T>(this Entity entity, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake, new()
            => await AddComponentWithIdAsync<T>(entity, entity.Id, cancelToken, isFromPool);

        public static async UniTask<T> AddComponentAsync<T, P1>(this Entity entity, P1 p1, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1>, new()
            => await AddComponentWithIdAsync<T, P1>(entity, entity.Id, p1, cancelToken, isFromPool);

        public static async UniTask<T> AddComponentAsync<T, P1, P2>(this Entity entity, P1 p1, P2 p2, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1, P2>, new()
            => await AddComponentWithIdAsync<T, P1, P2>(entity, entity.Id, p1, p2, cancelToken, isFromPool);

        public static async UniTask<T> AddComponentAsync<T, P1, P2, P3>(this Entity entity, P1 p1, P2 p2, P3 p3, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1, P2, P3>, new()
            => await AddComponentWithIdAsync<T, P1, P2, P3>(entity, entity.Id, p1, p2, p3, cancelToken, isFromPool);

        public static async UniTask<T> AddChildAsync<T>(this Entity entity, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = entity;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded) { lifecycle.RegisterComponent(component); return component; }
            return null;
        }

        public static async UniTask<T> AddChildAsync<T, P1>(this Entity entity, P1 p1, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1>, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = entity;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded) { lifecycle.RegisterComponent(component); return component; }
            return null;
        }

        public static async UniTask<T> AddChildAsync<T, P1, P2>(this Entity entity, P1 p1, P2 p2, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1, P2>, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = entity;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1, p2);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded) { lifecycle.RegisterComponent(component); return component; }
            return null;
        }

        public static async UniTask<T> AddChildAsync<T, P1, P2, P3>(this Entity entity, P1 p1, P2 p2, P3 p3, CancellationToken cancelToken = default, bool isFromPool = false) where T : AsyncEntity, IEntityAwake<P1, P2, P3>, new()
        {
            if (cancelToken.IsCancellationRequested) return null;
            T component = (T)Entity.Create(typeof(T), isFromPool);
            component.Id = EntityIdGenerator.Instance.GenerateId();
            component.Parent = entity;
            var lifecycle = ComponentLifecycleManager.Instance;
            lifecycle.AwakeEntity(component, p1, p2, p3);
            await AsyncEntityInitialize<T>(entity, component, cancelToken);
            if (component.IsLoaded) { lifecycle.RegisterComponent(component); return component; }
            return null;
        }
    }
}
