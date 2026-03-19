namespace GameLogic
{
    public enum BulletDestroyReason
    {
        None = 0,
        Manual = 1,
        LifetimeExpired = 2,
        HitTarget = 3,
        OwnerMissing = 4,
        ModuleClear = 5,
    }

    public enum BulletCollisionShape
    {
        Sphere = 0,
        Ray = 1,
    }
}
