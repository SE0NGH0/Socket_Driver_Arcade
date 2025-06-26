# Socket_Driver_Arcade

실행 방법

Ubuntu 환경
1. vim lcd1602.c
2. vim Makefile
3. make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
4. scp lcd1602.ko pi@ip주소:/home/pi

Raspi 환경
1. sudo insmod lcd1602.ko
2. sudo chmod 666 /dev/lcd1602
3. dmesg | tail로 lcd1602 register 확인
4. sudo mknod /dev/lcd1602 c $major $minor(dmesg | tail로 확인)
5. vim server_final.c
6. gcc server_final.c -o server_final
7. vim client_final.c
8. gcc client_final.c -o client_final
9. ./server_final
10. ./client_final ip주소 - 플레이어 2명 접속
11. 게임 실행

# 멀티플레이어 터미널 미니 게임, LCD1602 & LED 하드웨어 피드백

이 프로젝트는 C로 구현한 간단한 멀티플레이어 미니게임 서버·클라이언트 프로그램과, Raspberry Pi의 하드웨어(LED 및 I2C LCD1602 디스플레이)를 이용해 플레이어 점수 및 라운드 승자를 시각적으로 출력하는 예제입니다.

## 목차

* [주요 기능](#주요-기능)
* [하드웨어 구성](#하드웨어-구성)
* [소프트웨어 요구사항](#소프트웨어-요구사항)
* [프로젝트 구조](#프로젝트-구조)
* [빌드 방법](#빌드-방법)
* [사용법](#사용법)
* [코드 개요](#코드-개요)

## 주요 기능

* **3가지 미니게임**: 가위바위보, 연산 대결, 반응 속도 게임
* **두 명 동시 플레이**: TCP 클라이언트 2명이 각 라운드에 참여
* **자동 점수 집계**: 3라운드 종료 후 승/패 결과 요약
* **하드웨어 피드백**:

  * **LED**: 각 라운드별 승리 시 해당 라운드 LED(총 3개) 점등
  * **LCD1602**: 최종 점수 요약을 I2C LCD1602에 표시

## 하드웨어 구성

* **LED (GPIO 제어)**

  * 라운드1→GPIO17 (LED0)
  * 라운드2→GPIO27 (LED1)
  * 라운드3→GPIO22 (LED2)
  * 각 LED는 330Ω 저항과 함께 Raspberry Pi GND에 연결
* **I2C LCD1602 디스플레이**

  * SDA → GPIO2
  * SCL → GPIO3
  * 모듈 주소: 0x27
  * 4.7kΩ 풀업 저항 (보드 내장 또는 외부)

## 소프트웨어 요구사항

* Raspberry Pi OS (Raspbian 등)
* `gcc`, `make`, `pthread` 라이브러리
* `i2c-tools` (I2C 버스 확인 및 디버깅)
* `raspi-gpio` 유틸리티 (GPIO 제어)
* I2C 활성화:

  ```bash
  sudo raspi-config nonint do_i2c 0
  ```

## 프로젝트 구조

```
project/
├── server_final.c   # 게임 서버 및 LCD/LED 제어 (라운드별 LED + LCD)
├── client_final.c        # 게임 클라이언트 (터미널 인터페이스)
├── lcd1602.c           # I2C LCD1602 커널 모듈
└── Makefile            # 빌드 스크립트
```

## 빌드 방법

1. 프로젝트 디렉터리를 Raspberry Pi로 복사
2. 의존성 설치:

   ```bash
   sudo apt update
   sudo apt install build-essential i2c-tools raspi-gpio
   ```
3. 빌드:

   ```bash
   make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
   ```

   → `server_final`, `client_final`, `lcd1602.ko` 생성

## 사용법

### 1. LCD 커널 모듈 로드

```bash
sudo insmod lcd1602.ko
sudo chmod 666 /dev/lcd1602
sudo mknod /dev/lcd1602 c $MAJOR $MINOR (dmesg | tail에서 번호 확인)
ls -l /dev/lcd1602
```

### 2. 서버 실행

```bash
sudo ./server_final
```

* 포트 10000에서 두 명의 클라이언트 연결을 대기

### 3. 클라이언트 접속

두 개의 터미널에서:

```bash
./client_final <서버_IP>
```

### 4. 하드웨어 피드백 확인

* **라운드별 LED**: 각 라운드 종료 시 플레이어1이 이긴 라운드 LED만 점등
* **LCD1602**: 3라운드 종료 후 다음 내용 표시:

  ```
  P1: X win Y lose
  P2: A win B lose
  ```

## 코드 개요

### `server_final.c`

1. **네트워크**: TCP 소켓 생성, 포트 10000 바인딩, 최대 2명 접속
2. **미니게임 로직**

   * `play_rps()`, `play_math()`, `play_react()` 함수
3. **라운드별 LED 제어**

   * `round_winners[3]`에 각 라운드 승자(0 또는 1) 저장
   * `led_per_round()` 함수에서 승자 배열 참조, `raspi-gpio`로 GPIO17/27/22 제어
4. **LCD 제어**

   * `/dev/lcd1602`에 write하여 I2C LCD1602 출력
5. **결과 전송 및 정리**

### `client_final.c`

* 서버 연결 및 게임 입력/출력 처리

### `lcd1602.c`

* I2C LCD1602 커널 모듈 (dynamic char device)

---

