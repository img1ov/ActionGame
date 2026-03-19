using UnityEngine;

namespace GameLogic
{
    public class GameTimer : Singleton<GameTimer>
    {
        private float _timeScale = 1f;
        private bool _isPaused;
        private float _fixedDeltaTime = 0.02f;
        private float _fixedTimeAccumulator;
        private int _maxFixedStepsPerFrame = 10;

        public float TimeScale
        {
            get => _timeScale;
            set => _timeScale = Mathf.Max(0f, value);
        }

        public bool IsPaused
        {
            get => _isPaused;
            set => _isPaused = value;
        }

        public float FixedDeltaTime
        {
            get => _fixedDeltaTime;
            set => _fixedDeltaTime = Mathf.Max(0.001f, value);
        }

        public float DeltaTime { get; private set; }
        public float UnscaledDeltaTime { get; private set; }

        public void Update(float rawDeltaTime, float rawUnscaledDeltaTime)
        {
            UnscaledDeltaTime = rawUnscaledDeltaTime;

            if (_isPaused) { DeltaTime = 0f; return; }

            DeltaTime = rawDeltaTime * _timeScale;
            ComponentLifecycleManager.Instance?.UpdateAll(DeltaTime);
        }

        public void FixedUpdate(float rawFixedDeltaTime)
        {
            if (_isPaused) return;

            float scaledFixedDelta = rawFixedDeltaTime * _timeScale;
            _fixedTimeAccumulator += scaledFixedDelta;

            int steps = 0;
            while (_fixedTimeAccumulator >= _fixedDeltaTime && steps < _maxFixedStepsPerFrame)
            {
                ComponentLifecycleManager.Instance?.FixedUpdateAll(_fixedDeltaTime);
                _fixedTimeAccumulator -= _fixedDeltaTime;
                steps++;
            }

            if (steps >= _maxFixedStepsPerFrame) _fixedTimeAccumulator = 0f;
        }

        public void LateUpdate()
        {
            if (_isPaused) return;
            ComponentLifecycleManager.Instance?.LateUpdateAll();
        }

        protected override void OnRelease()
        {
            _fixedTimeAccumulator = 0f;
        }
    }
}
