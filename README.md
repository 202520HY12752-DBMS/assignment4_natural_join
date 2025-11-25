# Assignment 4 - Implementing Natural Join

이번 과제에서는 지난번에 구현한 disk-based B+tree 에다가 join operation을 구현합니다. 기본적으로 완성된 disk-based B+tree 코드는 제공되나 본인이 구현한 코드를 바탕으로 진행해도 됩니다. 가산점은 없습니다.

<b><i>단, 본인이 구현한 코드를 바탕으로 진행할 경우 repository의 `src/dbbpt/main.c`, `src/dbbpt/dbbpt.c`, `src/dbbpt/dbbpt.h`에 join operator를 위해 추가된 코드를 참고하여 API 형식을 반드시 맞춰줘야 합니다.</i></b>

1. PDF로 제공된 Assignment 4의 GitHub Classroom repository 초대 링크를 통해 자신의 repository를 생성합니다.
2. Repository의 이름을 assignment4-[본인 학번] 으로 변경합니다.
3. 해당 repository를 clone 받습니다.
4. README.md에 본인의 이름과 학번을 기입합니다.
5. PDF에 있는 페이지 설명을 바탕으로 `src/dbbpt/dbbpt.c`의 `db_join()` 함수를 완성합니다.
6. <b><i>채점 시 최대 메모리 사용량을 64MiB로 제한합니다.</i></b> 해당 환경에서 실행되지 않을 경우 감점 있습니다.
---

## 🏷️ 본인의 이름과 학번을 적어주세요
```
학번:
이름:
```

## 🗓️ 제출 기한
- <b style='background-color: #ffdce0'>2025년 12월 8일 (월) 23시 59분</b>
- <b style='background-color: #ffdce0'>기한 이후 제출 시 3시간 마다 전체 득점에서 5%씩 감점합니다.</b>

## 🛠️ 빌드 및 실행

### 전체 빌드

`make` 또는 `make all` 명령어로 모든 실행 파일을 한 번에 빌드할 수 있습니다.

```bash
make all
```

### 부분 빌드

특정 목표만 빌드할 수도 있습니다.

- `make inmembpt`: 메모리 기반 B+ Tree 예제(`inmembpt`)를 빌드합니다.
- `make dbbpt`: 직접 구현한 `file_manager.c`와 `dbbpt.c`를 사용하여 `dbbpt`를 빌드합니다.

>빌드된 모든 실행 파일은 `bin/` 디렉토리에 생성됩니다.

### 실행 파일 안내

#### 1. `./bin/inmembpt`

- **설명:** 메모리에서 동작하는 B+ Tree 예제입니다.
- **실행:**
  ```bash
  # order 값을 지정하여 실행 (기본값: 4)
  ./bin/inmembpt [<order>]
  ```
- **명령어:** `?`를 입력하여 도움말을 확인하세요.

#### 2. `./bin/dbbpt`

- **설명:** 직접 구현한 디스크 기반 B+ Tree입니다.
- **실행:**
  ```bash
  ./bin/dbbpt
  ```
- <b>메모리 제한 실행 <i style='color: #f7001dff'>(new)</i></b>:
  ```bash
  # 해당 터미널 세션에서 실행하는 프로세스의 가상 메모리 사용량을 65536KiB (64MiB)로 제한
  ulimit -v 65536
  # 완성한 프로그램 실행
  ./bin/dbbpt
  ```
- **사용법:**
  1.  `o <path> [l_ord] [i_ord]` 명령어로 데이터베이스 파일을 엽니다. (없으면 새로 생성)
  2.  `i`, `f`, `d` 등의 명령어로 데이터를 조작합니다.
  3.  `c` 명령어로 현재 파일을 닫고, 다시 `o`를 이용해 다른 파일을 열 수 있습니다.
  4.  `e <path> [echo] [resp]` 명령어로 외부 파일에 있는 명령어를 한번에 실행할 수 있습니다.

- <b>join operator <i style='color: #f7001dff'>(new)</i></b>:
  - `j <tree_path1> <tree_path2> <out_path>` 명령어로 두 tree 파일을 key를 기준으로 natural join한 결과를 out_path에 위치한 파일에 저장합니다.
  - 해당 명령어는 tree가 열려있는 상태에서는 사용할 수 없으므로 `c` 명령어로 현재 파일을 닫고 수행해야합니다.

- **명령어:** `?`를 입력하여 도움말을 확인하세요.
  > 채점은 이 실행 파일을 기준으로 진행됩니다.

---

## ✅ 테스트

제공된 테스트 케이스(`tc.txt`, `tc_lg.txt`, `tc_join.txt`, `tc_join_lg.txt`)를 통해 구현한 코드를 테스트할 수 있습니다.

### 테스트 환경 초기화

테스트 실행 전, `test_out` 디렉토리를 초기화해야 합니다.

```bash
make reset_test_env
```

### 테스트 케이스 실행

`dbbpt` 또는 `file_manager_tester`의 프롬프트에서 `e` 명령어를 사용하여 테스트 파일을 실행할 수 있습니다.

```bash
# 1. dbbpt 실행
./bin/dbbpt

# 2. 프롬프트에서 tc.txt 실행
> e tc.txt
```

> `e tc.txt 1 1` 처럼 `echo`와 `response`를 켜서 좀 더 자세한 실행 결과를 얻을 수도 있습니다.
> `tc_lg.txt`는 다량의 명령어를 실행하기 때문에 `echo`와 `response`를 켜지 마세요!

### `tc_join.txt` 테스트 케이스 <i style='color: #f7001dff'>(new)</i>

아래와 같은 두 개의 tree 를 `test_out`에 생성하고 natural join 한 결과물을 `test_out/join_test_out.txt`에 저장하는 테스트 케이스입니다.

```
tree1: (18, eighteen), (17, seventeen), (100, hunnit), (50, fifty), (31, thirty-one), (13, thirteen)
tree2: (11, ship-ill), (13, ship-sam), (100, baek), (10, ship), (1, ill), (0, BBang)
```

### `tc_lg_join.txt` 테스트 케이스 <i style='color: #f7001dff'>(new)</i>

`test_trees`에 저장된 두 tree를 natural join 한 결과물을 `test_out/join_test_out.txt`에 저장하는 테스트 케이스입니다. 각 트리에 저장된 레코드들은 아래와 같습니다.

```
lg_join_test1.tree: (1, AAA...A_1), (2, AAA...A_2), (3, AAA...A_3), ..., (1048194, AAA...A_1048194)
lg_join_test2.tree: (3, BBB...B_3), (5, BBB...B_5), (7, BBB...B_7), ..., (2097153, AAA...A_2097153)
```

> 이 테스트 케이스는 `test_trees`에 트리 파일이 없으면 동작하지 않습니다.
> 이 테스트 케이스는 십 분 이상 걸릴 수 있습니다.

### `lg_join_test_tree_maker.txt` <i style='color: #f7001dff'>(new)</i>

`tc_lg_join.txt` 에서 사용하는 tree 를 제작하는 테스트 케이스 입니다. `test_trees`에 이미 트리 파일이 제작되어 있어 따로 사용할 필요는 없지만 혹시 커스텀 테스트 케이스가 필요하다면 이 파일을 참조해서 커스텀 테스트 케이스를 만들어보세요!

### 커스텀 테스트 케이스

`tc.txt` 파일과 같이 명령어 목록을 작성하여 자신만의 테스트 케이스를 만들 수 있습니다. `#`으로 시작하는 라인은 주석 대신 화면에 해당 텍스트를 출력하는 용도로 사용할 수 있습니다.

---

## 🧹 빌드 정리

빌드 과정에서 생성된 `bin` 디렉토리와 실행 파일들을 삭제합니다.

```bash
make clean
```
