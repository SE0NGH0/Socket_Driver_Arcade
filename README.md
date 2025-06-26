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
5. vim server_lcd.c
6. gcc server_lcd.c -o server_lcd
7. vim client_lcd.c
8. gcc client_lcd.c -o client_lcd
9. ./server_lcd
10. ./client_lcd ip주소 - 플레이어 2명 접속
11. 게임 실행

# 멀티플레이어 터미널 미니 게임과 LCD1602 하드웨어 피드백

이 프로젝트는 C로 구현한 간단한 멀티플레이어 미니게임 서버·클라이언트 프로그램과, Raspberry Pi의 I2C 기반 LCD1602 디스플레이를 이용해 플레이어 점수 요약을 시각적으로 출력하는 예제입니다.

## 목차

* [주요 기능](#주요-기기)
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
* **LCD 디스플레이 출력**: I2C LCD1602로 최종 점수 요약 출력

## 하드웨어 구성

* Raspberry Pi I2C 핀
  * SDA → GPIO2
  * SCL → GPIO3
* I2C LCD1602 모듈 (I2C 주소 0x27)
* 4.7kΩ 풀업 저항 (I2C 라인에 내장되어 있을 수 있음)

## 소프트웨어 요구사항

* Raspberry Pi OS (Raspbian 등)
* `gcc`, `make`, `pthread` 라이브러리
* `i2c-tools` 유틸리티 (I2C 버스 확인용)
* I2C 모듈 활성화 (`raspi-config` 또는 `/boot/config.txt`에 `dtparam=i2c_arm=on`)

## 프로젝트 구조

```
project/
├── server_lcd.c           # 게임 서버 및 LCD 제어 로직
├── client_lcd.c           # 게임 클라이언트 (터미널 인터페이스)
├── lcd1602.c          # I2C LCD1602 커널 모듈
└── Makefile           # 빌드 스크립트
```

## 빌드 방법

1. 프로젝트 디렉터리를 Raspberry Pi로 복사
2. 의존성 설치:
   ```bash
   sudo apt update
   sudo apt install build-essential i2c-tools
   ```
3. I2C 활성화 확인:
   ```bash
   ls /dev/i2c-1
   ```
4. 빌드:
   ```bash
   make -j12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
   ```
   → `server_lcd`, `client_lcd`, `lcd1602.ko` 생성

## 사용법

### 1. LCD 커널 모듈 로드

```bash
sudo insmod lcd1602.ko
# /dev/lcd1602 장치 노드 생성 확인
ls -l /dev/lcd1602
``` 

### 2. 서버 실행

```bash
sudo ./server_lcd
``` 

* 포트 10000에서 두 명의 클라이언트 연결을 대기합니다.

### 3. 클라이언트 접속

두 개의 터미널을 열고 다음을 실행:

```bash
./client_lcd <서버_IP>
```

* 각 클라이언트는 3개의 미니게임을 순서대로 플레이합니다.

### 4. LCD 출력

3라운드가 끝나면 서버가 각 플레이어의 승/패 수를 계산하여 아래 형식으로 LCD1602에 출력합니다:

```
<첫 번째 줄>  P1: X win Y lose
<두 번째 줄>  P2: A win B lose
```

## 코드 개요

### `server_lcd.c`
1. **네트워크**: TCP 소켓 생성, 포트 10000 바인딩, 최대 2명 accept
2. **미니게임**:
   * 가위바위보
   * 연산 대결 (랜덤 연산 문제 + 응답 속도)
   * 반응 속도 (랜덤 딜레이 후 ENTER)
3. **점수 처리**: `game.scores[0]`(플레이어1), `game.scores[1]`(플레이어2)
4. **LCD 제어**: 최종 점수 문자열을 `/dev/lcd1602`에 write

### `client_lcd.c`
* 서버 연결 → `fgets`/`fprintf` 로 게임 프롬프트와 응답 처리
* 승리/패배/무승부 메시지 출력
* 3라운드 종료 후 서버 요약 메시지 수신

### `lcd1602.c`
* I2C 버스로 LCD1602 초기화 및 제어 드라이버
* `/dev/lcd1602`에 write 시 문자열을 화면에 표시
* 동적 `alloc_chrdev_region`, `cdev_add` 사용
* 기본 명령(rs, enable, 데이터) 및 초기화 시퀀스 구현

--- 
