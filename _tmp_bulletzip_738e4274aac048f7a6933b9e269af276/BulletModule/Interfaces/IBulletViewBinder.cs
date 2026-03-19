using UnityEngine;

namespace GameLogic
{
    public interface IBulletViewBinder
    {
        void Bind(BulletEntity bullet, GameObject viewInstance);
        void Unbind(BulletEntity bullet, GameObject viewInstance);
    }
}
