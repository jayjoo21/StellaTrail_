# Stella Trail ✦ 별의 여정

외계 행성에 불시착한 꼬마 우주인이 흩어진 비행선 부품을 수집해 고향으로 돌아가는 2D 탑다운 힐링 탐험 게임.

## 빌드 방법

### 방법 1: vcpkg (권장)

```powershell
# vcpkg 설치 (이미 있으면 skip)
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# 프로젝트 빌드
cd C:\code\StellaTrail
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
.\build\Release\StellaTrail.exe
```

### 방법 2: MSYS2 / MinGW

```bash
# MSYS2 패키지 설치
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-cmake

# 빌드
bash build_msys2.sh
./build/StellaTrail.exe
```

### 방법 3: 수동 SDL2 다운로드 (Windows)

```powershell
# 의존성 자동 다운로드
.\setup_deps.ps1

# 빌드
cmake -B build
cmake --build build --config Release
```

## 폰트 설정

한글 텍스트 표시를 위해 TTF 폰트가 필요합니다:
```
assets/fonts/NotoSansKR.ttf
```
`setup_deps.ps1` 실행 시 자동으로 다운로드됩니다. 없어도 게임은 실행되지만 텍스트가 표시되지 않습니다.

## 조작법

| 키 | 동작 |
|---|---|
| WASD / 방향키 | 이동 |
| 이동 + 바위 접촉 | 바위 밀기 |
| ESC | 타이틀로 돌아가기 |
| Space / Enter | 확인 / 게임 시작 |

## 게임플레이

- 지도를 탐험하며 빛나는 **부품 3개**를 수집하세요.
- **바위**를 밀어 **압력판(노란 타일)**에 올려놓으면 **문**이 열립니다.
- 오른쪽 패널의 비행선이 부품을 모을수록 단계적으로 복원됩니다.
- 3개 모두 수집하면 엔딩 씬이 재생됩니다.

## 물리 파라미터

| 행성 | 중력 | 마찰 | 충격량 배율 |
|---|---|---|---|
| 행성 1 (기본) | 9.8 | 0.85 | 1.0 |
| 행성 2 (저중력) | 3.0 | 0.3 | 2.5 |

`Game.cpp`의 `loadLevel()`에서 `PlanetPhysics` 구조체로 변경 가능합니다.

## 기술 스택

- **언어**: C++17
- **라이브러리**: SDL2, SDL2_image, SDL2_ttf
- **빌드**: CMake 3.16+
- **물리**: AABB 충돌 + 임펄스 기반 강체 시뮬레이션
