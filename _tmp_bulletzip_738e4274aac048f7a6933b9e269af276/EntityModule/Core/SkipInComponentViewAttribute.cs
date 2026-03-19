using System;

namespace GameLogic
{
    [AttributeUsage(AttributeTargets.Field)]
    public class SkipInComponentViewAttribute : Attribute
    {
        public string Reason { get; }
        public SkipInComponentViewAttribute(string reason = "") { Reason = reason; }
    }
}
