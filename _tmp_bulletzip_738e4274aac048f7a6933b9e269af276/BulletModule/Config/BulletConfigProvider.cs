using System;
using System.Collections.Generic;

namespace GameLogic
{
    internal sealed class BulletConfigProvider
    {
        private readonly Dictionary<string, BulletConfig> _configs = new(StringComparer.Ordinal);

        public bool Register(BulletConfig config)
        {
            if (config == null || !config.IsValid())
            {
                return false;
            }

            _configs[config.Id] = config;
            return true;
        }

        public bool TryGet(string configId, out BulletConfig config)
        {
            config = null;
            return !string.IsNullOrWhiteSpace(configId) && _configs.TryGetValue(configId, out config);
        }

        public void Clear()
        {
            _configs.Clear();
        }
    }
}
