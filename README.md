# MyGain — 첫 JUCE 플러그인

볼륨 노브 하나짜리 게인 플러그인. 목적은 "빌드 → Ableton에서 로드 → 소리 변화 확인"까지 전체 파이프라인을 한 번 완주하는 것.

## 1. 준비 (한 번만)

```bash
# Xcode Command Line Tools (없다면)
xcode-select --install

# CMake
brew install cmake
```

## 2. 빌드

프로젝트 폴더에서:

```bash
cmake -B build
cmake --build build --config Release
```

- 첫 빌드는 JUCE 다운로드 + 컴파일 때문에 5~10분 걸림. 이후엔 수십 초.
- 빌드가 끝나면 자동으로 설치됨:
  - AU → `~/Library/Audio/Plug-Ins/Components/MyGain.component`
  - VST3 → `~/Library/Audio/Plug-Ins/VST3/MyGain.vst3`

## 3. Ableton에서 확인

1. Live 재시작 (플러그인 스캔은 시작 시에만)
2. Settings → Plug-Ins 에서 VST3 / AU 켜져 있는지 확인, Rescan
3. 브라우저 → Plug-Ins → Shane → MyGain 을 트랙에 드롭
4. Gain 노브를 돌려서 볼륨이 변하면 성공 🎉

Standalone 버전도 빌드되니 Live 없이 빠르게 테스트할 땐
`build/MyGain_artefacts/Release/Standalone/MyGain.app` 실행.

## 4. 코드 읽는 순서

1. `Source/PluginProcessor.h` — 전체 구조 파악 (주석 참고)
2. `PluginProcessor.cpp`의 `createParameterLayout()` — 파라미터 정의하는 법
3. `processBlock()` — 심장부. 여기만 이해하면 절반은 끝

## 5. 디스토션으로 가는 길

`processBlock()` 안에 주석 처리된 한 줄이 있음:

```cpp
data[sample] = std::tanh (data[sample] * 4.0f);
```

이걸 풀고 다시 빌드하면 게인 플러그인이 새추레이터가 됨. 그 다음 단계:

1. `4.0f`를 "Drive" 파라미터로 만들기 (gain 파라미터 만든 방식 복사)
2. 출력 볼륨 보상용 "Output" 파라미터 추가
3. `tanh(x)` vs `x - x³/3` vs `x*|x|` 커브 비교 → 배음 구조가 어떻게 다른지 스펙트럼으로 확인
4. 짝수 배음: 커브에 비대칭 추가 → `tanh(x + 0.3f)` 같은 오프셋
5. 앨리어싱 방지: `juce::dsp::Oversampling` (여기부터가 진짜 공부)

## 막히면

에러 메시지를 그대로 Claude에 붙여넣기. C++ 에러는 길고 무섭게 생겼지만 보통 첫 줄에 답이 있음.
