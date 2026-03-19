using System;

namespace GameLogic
{
    public struct EntityRef<T> where T : Entity
    {
        private readonly long _instanceId;
        private T _entity;

        private EntityRef(T t)
        {
            if (t == null) { _instanceId = 0; _entity = null; return; }
            _instanceId = t.InstanceId;
            _entity = t;
        }

        private T UnWrap
        {
            get
            {
                if (_entity == null) return null;
                if (_entity.InstanceId != _instanceId) _entity = null;
                return _entity;
            }
        }

        public static implicit operator EntityRef<T>(T t) => new EntityRef<T>(t);
        public static implicit operator T(EntityRef<T> v) => v.UnWrap;
    }
}
