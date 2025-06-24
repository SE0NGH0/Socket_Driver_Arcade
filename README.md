# server_client_device_driver_module_project

실행 방법

Ubuntu 환경
1. vim led.c
2. vim Makefile
3. make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
4. scp led.ko pi@ip주소:/home/pi

Raspi 환경
1. sudo insmod led.ko
2. sudo chmod 666 /dev/led_control
3. dmesg | tail로 led_control register 확인
4. vim server.c
5. gcc server.c -o server
6. vim client.c
7. gcc client.c -o client
8. sudo ./server
9. sudo ./client ip주소 - 플레이어 2명 접속
10. 게임 실행

# 멀티플레이어 터미널 미니 게임과 LED 하드웨어 피드백

이 프로젝트는 C로 구현한 간단한 멀티플레이어 미니게임 서버·클라이언트 프로그램과, Raspberry Pi의 GPIO LED를 이용해 플레이어 1의 승리 수를 시각적으로 표시하는 예제입니다.

## 목차

* [주요 기능](#주요-기능)
* [하드웨어 구성](#하드웨어-구성)
* [소프트웨어 요구사항](#소프트웨어-요구사항)
* [프로젝트 구조](#프로젝트-구조)
* [빌드 방법](#빌드-방법)
* [사용법](#사용법)

  * [1. LED 커널 모듈 로드 (선택)](#1-led-커널-모듈-로드-선택)
  * [2. 서버 실행](#2-서버-실행)
  * [3. 클라이언트 접속](#3-클라이언트-접속)
  * [4. LED 피드백](#4-led-피드백)
* [코드 개요](#코드-개요)

  * [`server.c`](#serverc)
  * [`client.c`](#clientc)
  * [`led_control.c` (선택적 커널 모듈)](#led_controlc-선택적-커널-모듈)
* [라이선스](#라이선스)

## 주요 기능

* **3가지 미니게임**: 가위바위보, 연산 대결, 반응 속도 게임
* **두 명 동시 플레이**: TCP 클라이언트 2명이 각 라운드에 참여
* **자동 점수 집계**: 3라운드 종료 후 승/패 결과 요약
* **하드웨어 피드백**: 플레이어 1의 승리 수(0\~3)를 LEDs로 표시

## 하드웨어 구성

* Raspberry Pi GPIO 핀:

  * LED0 → GPIO 17
  * LED1 → GPIO 27
  * LED2 → GPIO 22
* 각 LED는 적절한 저항과 함께 Raspberry Pi GND에 연결되어야 합니다.

## 소프트웨어 요구사항

* Raspberry Pi OS (Raspbian 등)
* `gcc`, `make`, `pthread` 라이브러리
* `raspi-gpio` 유틸리티 (GPIO 제어용)

## 프로젝트 구조

```
project/
├── server.c           # 게임 서버 및 LED 제어 로직
├── client.c           # 게임 클라이언트 (터미널 인터페이스)
├── led_control.c      # (선택) /dev/led_control 커널 모듈
└── Makefile           # 빌드 스크립트
```

## 빌드 방법

1. 프로젝트 디렉터리를 Raspberry Pi로 복사
2. 의존성 설치:

   ```bash
   sudo apt update
   sudo apt install build-essential raspi-gpio
   ```
3. 빌드:

   ```bash
   make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
   ```

   → `server`, `client`, `led_control.ko` 생성

## 사용법

### 1. LED 커널 모듈 로드 (선택)

> LED 제어를 커널 모듈 방식으로 하고 싶을 경우에만 진행

```bash
sudo insmod led_control.ko
# 비root 사용자도 쓰기 권한이 필요하다면
sudo chmod 666 /dev/led_control
```

### 2. 서버 실행

```bash
sudo ./server
```

* 포트 10000에서 두 명의 클라이언트 연결을 대기합니다.

### 3. 클라이언트 접속

두 개의 터미널을 열고 다음을 실행:

```bash
./client <서버_IP>
```

* 각 클라이언트는 3개의 미니게임을 순서대로 플레이합니다.

### 4. LED 피드백

3라운드가 끝나면 서버가 플레이어 1의 승리 수를 계산하여 아래와 같이 GPIO를 설정합니다:

| 승리 수 | 비트마스크 | GPIO 상태             |
| ---- | ----- | ------------------- |
| 0    | 000   | 모든 LED OFF          |
| 1    | 001   | LED0 ON             |
| 2    | 011   | LED0, LED1 ON       |
| 3    | 111   | LED0, LED1, LED2 ON |

GPIO 제어 예시:

```c
system("raspi-gpio set 17 op dh"); // ON
system("raspi-gpio set 27 op dl"); // OFF
// ...
```

## 코드 개요

### `server.c`

1. **네트워크**: TCP 소켓 생성, 포트 10000 바인딩, 최대 2명 accept
2. **미니게임**:

   * 가위바위보: 입력 문자열 비교
   * 연산 대결: 랜덤 연산 문제, 정답 및 응답 속도로 승부
   * 반응 속도: 랜덤 딜레이 후 메시지, 빠른 엔터로 승부
3. **점수 처리**: `game.scores[0]`(플레이어1), `game.scores[1]`(플레이어2)
4. **LED 제어**: 최종 승리 수에 따른 `raspi-gpio` 시스템 호출

### `client.c`

* 서버 연결 → `fgets`/`fprintf` 로 게임 프롬프트와 응답 처리
* 승리/패배/무승부 메시지 출력
* 3라운드 종료 후 서버 요약 메시지 수신

### `led_control.c` (선택적 커널 모듈)

* `/dev/led_control`에 `echo <0~7> >` 입력 시 LED 비트마스크 제어
* 동적 `gpio_request()` 및 `gpio_set_value()` 사용
