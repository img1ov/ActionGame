using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace GameLogic
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct IdStruct
    {
        public short Process;
        public uint Time;
        public uint Value;

        public long ToLong()
        {
            ulong result = 0;
            result |= (ushort)this.Process;
            result <<= 30;
            result |= this.Time;
            result <<= 20;
            result |= this.Value;
            return (long)result;
        }

        public IdStruct(uint time, short process, uint value)
        {
            this.Process = process;
            this.Time = time;
            this.Value = value;
        }

        public IdStruct(long id)
        {
            ulong result = (ulong)id;
            this.Value = (uint)(result & EntityIdGenerator.Mask20bit);
            result >>= 20;
            this.Time = (uint)result & EntityIdGenerator.Mask30bit;
            result >>= 30;
            this.Process = (short)(result & EntityIdGenerator.Mask14bit);
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct InstanceIdStruct
    {
        public uint Time;
        public uint Value;

        public long ToLong()
        {
            ulong result = 0;
            result |= this.Time;
            result <<= 32;
            result |= this.Value;
            return (long)result;
        }

        public InstanceIdStruct(uint time, uint value)
        {
            this.Time = time;
            this.Value = value;
        }

        public InstanceIdStruct(long id)
        {
            ulong result = (ulong)id;
            this.Value = (uint)(result & uint.MaxValue);
            result >>= 32;
            this.Time = (uint)(result & uint.MaxValue);
        }
    }

    internal class EntityIdGenerator : Singleton<EntityIdGenerator>
    {
        public const int Mask14bit = 0x3fff;
        public const int Mask30bit = 0x3fffffff;
        public const int Mask20bit = 0xfffff;

        private long _epoch2022;
        private int _value;
        private int _instanceIdValue;

        protected override void OnInit()
        {
            long epoch1970tick = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).Ticks / 10000;
            _epoch2022 = new DateTime(2022, 1, 1, 0, 0, 0, DateTimeKind.Utc).Ticks / 10000 - epoch1970tick;
        }

        private uint TimeSince2022()
        {
            long now = (DateTime.UtcNow.Ticks - new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).Ticks) / 10000;
            return (uint)((now - _epoch2022) / 1000);
        }

        public long GenerateId()
        {
            uint time = TimeSince2022();
            int v;
            lock (this)
            {
                if (++_value > Mask20bit - 1) _value = 0;
                v = _value;
            }
            return new IdStruct(time, 0, (uint)v).ToLong();
        }

        public long GenerateInstanceId()
        {
            uint time = TimeSince2022();
            uint v = (uint)Interlocked.Add(ref _instanceIdValue, 1);
            return new InstanceIdStruct(time, v).ToLong();
        }
    }
}
