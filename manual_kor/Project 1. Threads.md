#Project1. Threads

### TA session

- **Reading materials**

1. (2) Threads, (A.2) Threads, (A.3) Synchronization, (E) Debugging tools, 그 외 Backgrounds와 FAQ

2. (A.1) Loading, (A.2) Threads, (A.3) Synchronization, (A.6) Virtual Addresses, (B.1) Niceness, (B.7) Fixed-point real arithematic

3. (2.2.4) Advanced scheduler는 읽지 말기 

- **To do: thread.c 스켈레톤 코드 채우기**

1. **Alarm clock**총 6개의 테스트 통과하기
   - 대략 **TICKS** timer ticks 동안 execution 중지
   - 이는 Busywaiting을 피하기 위함이며, thread를 시간이 흐를동안 sleep - wake up 하기
   - `src/devices/timer.c` 에 정의된 **timer_sleep()**을 reimplement
   - 관련 파일: `devices/timer.c`, `threads/thread.c`, `threads/thread.h`, `lib/kernel/list.c`
   - [HINT] 어떤식으로 timer tick이 증가하는지 알고 있어야 함
2. **Priority scheduling** 총 3개의 테스트 통과하기
   - 모든 thread는 주어진 scheduling priority가 존재
   - 이 중 ready queue 내부의 highest priority인 thread를 run next
   - priority는 64 레벨이며 (default 31), 낮은 숫자는 낮은 우선순위를 의미
   - 현재 pintos 시스템은 라운드 로빈 스케줄러 형태이지만, 이를 highest priority thread를 스케줄 하는 형태로 바꾸기
   - *void thread_set_priority (int new_priority)*, *void thread_set_priority (void)* 함수 활용하기
   - 관련 파일: `threads/thread.c`, `threads/thread.h`
3. Priority donation은 하지 말 것

- **Debugging with GDB**

1. 기본 실행 방법 
   1. pintos --gdb --run *testname*
   2. 다른 터미널 창을 띄워서 pintos-gdb kernel.o (kernel.o를 디버깅)
      - kernel.o 가 존재하는 threads/build 에서 실행
   3. target remote localhost:1234
      - 두 개의 터미널로 띄웠으니 remote localhost를 target으로 지정해줘서 access 해야함
2. breakpoint
   - (gdb) b page_fault 시 page_fault가 호출되기 직전에 execution pause
   - 이를 통해 register, variables, backtrace call stack 등 확인 가능
3. 이 외에 추가적인 내용은 E.5 GDB macros (misc/gdb-macros) 에서 확인 가능

###Background

 이 과제에서는 minimally functional thread system이 제공됩니다. 당신의 과제는 synchronization 문제를 더 잘 이해하기 위해 이 시스템의 functionality를 확장하는 것입니다. 이 과제는 주로 'threads' 디렉토리에서 진행되며, 어떤 작업은 'devices' 디렉토리에서도 진행됩니다. 컴파일은 'threads' 디렉토리에서 진행되어야 합니다.

- **Understanding Threads**

첫 번째 단계는 initial thread system 에 관한 코드를 읽고 이해하는 것입니다. 핀토스는 이미 thread creation, thread completion, thread switch를 위한 간단한 scheduler와  synchronization의 기초 요소들 (semaphores, locks, condition variables, optimization barrier) 을 구현해 놓았습니다.이 코드들의 일부는 약간 미스테리하게 보일 수 있습니다.  원한다면, (거의) 모든 곳에 `printf()` 를 추가해서 무엇이 일어나는 지, 어떤 순서로 진행되는지 볼 수 있습니다. 또한 커널을 디버거에서 실행시키고 궁금한 지점에 breakpoint를 설정해서 코드를 단계별로 실행할 수도 있습니다.

 Thread가 생성될 때, 당신은 스케쥴될 새로운 context를 생성하는 것이기도 합니다. 이 context에 실행될 함수는 `thread_create()` 의  argument로 넣어서 context에 제공할 수 있습니다. Thread가 처음 스케쥴되고 실행될 때, thread는 새로 생성된 context 내에서 `thread_create()`로 들어간 함수의 시작 부분부터 실행됩니다. 그리고 그 함수가 return할 때, thread는 종료됩니다. 즉, 각각의 thread는 핀토스 내에서 `thread_create()` 의 인자로 들어간 함수가 `main()` 함수 역할을 하는 미니 프로그램과 같다고 볼 수 있습니다.

 어떤 순간에도, 정확히 한 thread만이 실행되고, 나머지는 비활성화 됩니다. Scheduler는 어떤 thread가 다음으로 실행될지 결정합니다. (만일 실행될 준비가 된 thread가 하나도 없을 때에는, `idle()` 에서 실행되는 'idle' thread라는 특별한 thread가 실행됩니다.) Synchronization primitives들은 한 thread가 다른 thread가 무엇인가를 하는 것을 기다려야 할 때, context가 바뀌도록 할 수 있습니다.

 Context switch의 대한 코드는 `threads/switch,S` 파일에 있는데, 이는 80x86 어셈블리 코드입니다. (이 코드는 이해할 필요가 없습니다.) 이 코드는 현재 실행중인 thread의 state를 저장하고, 바꾸려고 하는 thread의 state를 복구합니다. (다시 가져옵니다.)

 GDB 디버거를 사용하면, context switch를 따라가면서 무슨 일이 일어나는 지 볼 수 있습니다. ([GDB 사용법]()) `schedule()` 에 breakpoint를 설정하고 시작하면, 그 부분에서부터 단계별 실행을 할 수 있습니다. 한 thread가 `switch_threads()`를 call 하면, 다른 thread가 실행되기 시작하고, 새로운 thread가 가장 먼저하는 것은 `switch_threads()` 에서 반환하는 것임을 알게 될 것입니다. 왜, 그리고 어떻게 called 된 `switch_threads()` 가 return하는 `switch_threads()`와 다르다는 것을 이해하기만 한다면, thread 시스템을 이해하게 될 것입니다. 더 자세한 정보는 [Thread Switching]() 에서 얻을 수 있습니다.

**주의**: 핀토스에서, 각각의 thread는 4kB보다 작은 사이즈의 작고 고정된 크기의 실행 스텍에 할당됩니다. 커널은 stack overflow를 열심히 감지하지만, 이는 완벽하지 않습니다. 만일 non-static local 변수 (ex 'int buf[1000]') 와 같은 커다란 데이터 구조를 선언하면, 알 수 없는 커널 패닉과 같은 기괴한 오류를 발생시킬 수 있습니다. 이에 대한 대안책은 page allocator 와 block allocator를 포함한 stack allocation 입니다. ([Memory Allocation]())

- **Source Files: thread.c와 thread.h 중요**

`threads/` 디렉토리의 파일들에 대한 간단한 개요입니다. 

`loader.S`, `loader.h`
커널 로더 입니다. 자세한 내용은 [A.1.1 The Loader]()에서 확인할 수 있습니다. **이 코드는 보거나 수정할 필요가 없습니다.**

`start,S`
80x86 CPU에서 메모리 보호 및 32 비트 작동에 필요한 기본 설정을 수행합니다. Loader와는 다르게, 이 코드는 커널의 실제 파트입니다. 자세한 내용은 [A.1.2 Low-Level Kernel Initialization]()을 참고하세요.

`kernel.lds.S`
**이 코드는 보거나 수정할 필요가 없습니다.**

`init.c`, `init.h`
커널의 메인 프로그램인 `main()` 을 포함하고 있는 Kernel initialization에 대한 코드입니다. 적어도 `main()` 는 보아야 합니다. 원한다면 initialization code를 본인이 직접 추가할 수 있습니다. [A.1.3 High-Level Kernel Initialization]() 을 참고하세요.

`thread.c`, `thread.h`
Basic Kernel 을 제공합니다. 대부분의 작업은 이 파일들에서 진행될 것입니다. `thread.h` 파일은 4개의 모든 프로젝트에서 수정할 thread struct를 정의합니다. [A.2.1 struct thread]() 와 [A.2 Threads]() 를 참고하세요.

`switch.S`, `switch.h`
위에서 다루었던 Switching threads를 위한 어셈블리 코드입니다. [A.2.2 Thread Functions]() 를 참고하세요.

`palloc.c`, `palloc.h`
4kB 페이지 배수의 시스템 메모리를  다루는 Page allocator 입니다. [A.5.1 Page Allocator]() 를 참고하세요.

`malloc.c`, `malloc.h`
커널을 위한 간단한 ` malloc()` 과 `free()` 함수가 구현되어 있는 코드입니다. [A.5.2 Block Allocator]() 를 참고하세요.

`interrupt.c`, `interrupt.h`
interrupt를 끄고 키는 것을 위한 Basic한 intteruupt handling 함수들이 정의된 코드입니다. [A.4 Interrupt Handling]() 을 참고하세요.

`intr-stubs.S`, `intr-stubs.h`
Low-level interrupt handling을 위한 어셈블리 코드입니다. [A.4.1 Interrupt Infrasturcture]() 를 참고하세요.

`synch.c`, `synch.h`
Basic synchronization primitives: semaphores, locks, condition variables, and optimization barriers. 4개의 모든 프로젝트에서 synchronization을 위해 사용될 것입니다. [A.3 Synchronization]() 을 참고하세요.

`io.h`
I/O port access를 위한 함수들이 정의되어 있습니다. 이 코드는 touch할 필요가 없을 `devices/` 디렉토리의 소스 코드에 의해 대부분 사용됩니다.

`vaddr.h`, `pte.h`
Virtual address와 페이지 테이블 등록을 위한 함수와 메크로가 정의되어 있습니다. 프로젝트 3에서 중요하게 사용되지만, 지금은 무시해도 됩니다.

`flags.h`
A few bits in the 80x86 "flags" register 를 정의하는 매크로입니다. 아마 관심이 안 가는 파일이었겠지만, 기쁘게도 보거나 수정할 필요가 없다고 합니다 :)

- **`devices/` code: timer.c와 timer.h 중요**

Basic threaded 커널은 `devices/` 디렉토리의 해당 파일들을 포함합니다.

`timer.c`, `timer.h`
default 값으로 1초당 100번 tick하는 시스템 타이머 입니다. **프로젝트 1에서 이 코드를 수정할 것입니다.**

`vga.c`, `vga.h`
VGA 디스플레이 드라이버 입니다. 화면에 작성한 텍스트가 나타나게 하는데 관여합니다. 이 코드를 볼 필요는 없습니다. `printf()` 함수가 여러분을 위해 VGA 디스플레이의 함수를 호출하기 때문에, 직접 이 코드를 호출할 이유는 거의 없습니다. 

`serial.c`, `serial.h`
Serial 포트 드라이버 입니다. 이 또한, `printf()` 함수가 여러분을 대신해 이 코드들을 호출하기 때문에, 이를 직접 할 필요가 없습니다. 이 친구는 serial input을 input layer로 전달하여 처리하는 역할을 합니다.

`block.c`, `block.h`
고정 크기 블록의 배열로 구성된 블록 장치, 즉 RAM 과 같은 disk-like 장치의 추상화 계층입니다. 기본적으로 Pintos는 두 가지 유형의 블록 장치인 IDE disk와 partition을 지원합니다. 타입에 상관없이 Block devices는 프로젝트2 전까지 사용되지 않습니다. 

`ide.c`, `ide.h`
최대 4 개의 IDE 디스크에서 섹터 읽기 및 쓰기를 지원합니다.

`partition.c`, `partition.h`
디스크의 partition 구조를 이해하여, 하나의 디스크를 여러 region (partition) 으로 나누어 독립적으로 사용할 수 있도록 합니다.

`kbd.c`, `kbd.h`
키보드 드라이버 입니다. 키 입력을 헨들링하여 input layer로 보냅니다.

`input.c`, `input.h`
Input layer 입니다. 키보드 또는 serial 드라이버에 의해 전달 받은 문자를 input하는 큐입니다.

`intq.c`, `intq.h`
커널 thread와 인터럽트 핸들러가 모두 액세스하려는 circular queue를 관리하기 위한 인터럽트 queue 입니다. 키보드와 serial 드라이버에서 사용됩니다.

`rtc.c`, `rtc.h`
커널이 현재 날짜와 시간을 결정할 수 있도록 해주는 Real-time clock 드라이버 입니다. 기본적으로 이는 `thread/init.c` 에서만 random number generator의 initial seed값을 고르기 위해 사용됩니다.

`speaker.c`, `speaker.h`
스피커가 소리를 내게 하는 드라이버입니다.

`pit.c`, `pit.h`
8254 Programmable Interrupt 타이머를 구성하는 코드입니다. 이 코드는 각 장치가 PIT의 출력 채널 중 하나를 사용하기 때문에 `devices/timer.c` 와 `devices/speaker.c`에서 사용됩니다.

* **`lib/` files**

`lib/` 디렉토리와 `lib/kernel/` 디렉토리는 유용한 라이브러리 루틴들을 포함하고 있습니다. 
(`lib/user/` 디렉토리 파일들은 프로젝트 2에서 시작하는 user program들에서 사용될 것입니다.)

`ctype.h`, 
`inttypes.h`, 
`limits.h`, 
`stdarg.h`, `stdbool.h`, `stddef.h`, `stdint.h`, 
`stdio.c`, `stdio.h`, 
`stdlib.c`, `sodlib.h`, 
`string.c`, `string.h`
Standard C 라이브러리의 subset입니다. 최근에 도입되어 접하지 못한 C 라이브러리의 정보를 얻고자 한다면, [C.2 C99]() 을 참고하세요. 안전성을 위해 의도적으로 제거된 내용을 확인하고 싶다면, [C.3 Unsafe String Functions]() 를 참고하세요.

`debug.c`, `debug.h`
디버깅을 돕는 함수와 매크로들이 정의되어 있습니다. [E. Debugging Tools]() 를 참고하세요.

`random.c`, `random.h`
랜덤한 숫자를 생성해주는 generator가 정의 되어있습니다. 아래의 세 가지 중 하나라도 수행하지 않는다면 랜덤한 값은 핀토스 실행마다 같은 순서로 나올 것입니다. (아래 중 하나만 선택하면 됩니다.)

1. 각 실행에서 -rs 커널 명령 행 옵션에 새로운 임의 시드 값을 지정
2. `pintos` 의 옵션으로 `-r` 옵션을 사용
3. Bochs 대신 다른 시뮬레이터 사용

`round.h`
rounding을 위한 매크로입니다.

`syscall-nr.h`
System call number 입니다. project 2전까지 사용되지 않습니다.

`kernel/list.c`, `kernel/list.h`
Doubly linked list가 구현되어 있습니다. 핀토스의 모든 코드에서 사용될 수 있으며, 프로젝트 1을 하면서도 이용하고 싶을 것입니다.

`kernel/bitmap.c`, `kernel/bitmap.h`
Bitmap이 구현되어 있습니다. 사용하고 싶다면 언제든 사용해도 좋지만, 프로젝트 1을 하는 동안에는 사용할 필요가 없을 것입니다.

`kernel/hash.c`, `kernel/hash.h`
Hash table이 구현되어 있습니다. 프로젝트 3에서 유용하게 사용될 것입니다.

`kernel/console.c`, `kernel/console.h`, `kernel/stdio.h`
`printf()` 를 포함한 몇몇 함수들이 정의되어 있습니다.

- **Synchronization**

 알맞은 synchronization은 이번 프로젝트의 솔루션에서 중요한 파트입니다. 모든 synchronization 문제들은 interrupt를 off함으로써 쉽게 해결될 수 있습니다. Interrupt를 off하면, concurrency(동시성)도 없어지기 때문에, race condition에 대한 가능성이 없기 때문입니다. 그렇기 때문에, 모든 synchronization 문제를 이 방법으로 해결하고 싶겠지만, **그러면 안됩니다**. 대신, **semaphore, lock과 condition variable (synchronization primitives)** 을 이용해 synchronization 문제를 해결하길 바랍니다. 어떤 상황에 어떤 synchronization primitives를 사용하는 것이 맞는 지 확실하지 않다면, [A.3 Synchronization]() 또는 **`threads/synch.c` 의 주석** 부분을 보시길 바랍니다.

 핀토스 프로젝트에서 인터럽트를 비활성화함으로써 가장 잘 해결되는 문제의 유일한 클래스는, 커널 스레드와 인터럽트 핸들러 간에 공유되는 data 를 조정하는 것입니다. 인터럽트 핸들러는 잠을 잘 수 없기 때문에 locks를 얻을 수 없습니다. 즉, 커널 스레드 및 인터럽트 핸들러 간에 공유되는 data는 인터럽트를 해제하여 커널 스레드 내에서 보호해야 합니다.

 이 프로젝트는 인터럽트 핸들러로부터 약간의 thread state만 액세스하면 됩니다. alarm clock의 경우 타이머 인터럽트가 잠자는 threads을 깨워야 합니다. advanced scheduler에서 타이머 인터럽트는 몇 가지 전역 및 thread당 변수에 액세스해야 합니다. 커널 스레드에서 이러한 변수에 액세스할 때 타이머 인터럽트가 간섭하지 않도록 인터럽트를 비활성화해야 합니다.

 인터럽트를 해제할 때는 가능한 한 최소한의 코드에 대해서만 주의를 기울여야 하며, 그렇지 않으면 timer ticks나 input events와 같은 중요한 것을 잃게 될 수 있습니다. 인터럽트를 끄면 인터럽트 처리 지연 시간도 증가하여 너무 오래 걸리면 기계가 느려질 수 있습니다.

 synch.c의 synchronization primitives 자체는 인터럽트를 비활성화함으로써 구현됩니다. 여기서 인터럽트를 비활성화한 상태에서 실행되는 코드의 양을 늘려야 할 수도 있지만, 그래도 최소한으로 유지하도록 노력해야 합니다.

 만약 코드의 한 부분이 중단되지 않도록 하려면, 인터럽트를 비활성화하는것은 디버깅에 유용할 수 있습니다. 프로젝트를 제출하기 전에 디버깅 코드를 제거해야 합니다. 

 busywaiting이 존재해서는 안됩니다. thread_yield() 로 불리는 타이트 루프는 busywaiting의 한 형태입니다.

- **Development Suggestions**

 과제 제출 직전에 팀원과 조각 모음을 하는 형태로 작업하지 마십시오. 그리고 버전관리 하는 것을 추천합니다. 디버깅 파트도 열심히 읽으시길 바랍니다. 

### Requirements

- **Design Document**

프로젝트를 시작하기에 앞서, design document template를 읽어보시는 것을 권장드립니다. 프로젝트 1에 대한 Design Document는 [여기](https://web.stanford.edu/class/cs140/projects/pintos/threads.tmpl), 그리고 전체 design document에 대한 가이드는 [D. Project Documentation]() 을 참고하세요.

- **Alarm Clock**

`devices/timer.c` 에 정의된 `timer_sleep()` 함수를 다시 구현하십시오. 현재는 'busy-wait' 방식, 즉 반복문을 통해, 시간이 지날 때까지 시간을 check하고, thread_yield()를 call하는 것을 반복하는 방식으로 구현되어 있습니다. 따라서 이 과제는 busy-wait 를 사용하지 않는 방법으로 재구현하는 것입니다.

> **void timer_sleep (int64_t ticks)**
>
> timer_sleep()를 호출하는 thread의 실행을 최소 ticks 값 만큼의 시간이 지난 이후까지 일시 중단합니다. 시스템이 idle 상태가 아닌 한, thread는 정확히 ticks 값 이후에 깨어날 필요는 없습니다. thread를 해당 시간동안 기다린 후 ready queue에 넣으면 됩니다.
>
> timer_sleep()는 실시간으로 작동하는 thread에 유용합니다. (ex 커서를 1초당 한번 씩 깜박이기)
>
> timer_sleep()의 argument는 millisecond나 다른 단위가 아닌, timer ticks로 명시됩니다. 1초당 100번의 timer ticks가 있습니다. 100 이라는 값은 `devices/timer.h` 에 정의된 TIMER_FREQ 값인데, 바꿀 수 는 있지만 바꾸게 되면 여러 테스트에서 오류가 발생할 수 있으므로 권장하지 않습니다.

`timer_msleep()`, `timer_usleep()`, `timer_nsleep()` 은 단위에 따른 sleep을 위해 구현되어 있는 함수로, 필요한 경우 `timer_sleep()` 가 알아서 call하기 때문에, 수정할 필요가 없습니다.

만일 delay가 너무 짧거나 길게 느껴진다면, pintos의 `-r` 옵션에 대한 설명을 다시 읽으십시오. ([1.1.4 Debugging versus Testing]())

Alarm clock 구현은 프로젝트 4에서는 유용할 수 있지만, 다른 프로젝트에는 영향을 주지 않습니다.

- **Priority Scheduling**

Pintos 에서 우선 순위 스케줄링을 구현하십시오. 현재 실행중인 thread보다 우선 순위가 높은 thread가 ready list에 추가되면, 현재 실행중인 thread는 즉시 프로세서를 새 thread에게 양보해야 합니다. 마찬가지로 thread가 lock, semaphore 또는 condition variable들을 기다리는 경우, 우선 순위가 가장 높은 대기 thread를 먼저 깨워야합니다. Thread는 언제든지 자체 우선 순위를 높이거나 낮출 수 있고, 현재 실행 중인 thread의 경우, 우선 순위가 가장 높지 않을 때까지 낮추게 된다면, 즉시 CPU 사용을 다른 thread에게로 넘겨주게 됩니다.

Thread의 우선 순위 범위는 `PRI_MIN` (0)부터 `PRI_MAX` (63)까지 입니다. 숫자가 낮을수록 우선 순위가 낮으므로 0이 가장 낮은 우선 순위, 63가 가장 높은 우선 순위를 갖습니다. Initial thread의 우선순위는 `thread_create()` 의 argument로 전달됩니다. 특정 우선 순위를 사용해야 하는 이유가 없다면, `PRI_DEFAULT`  (31)을 이용하십시오. PRI_ 매크로 상수는 `threads/thread.h` 에 정의되어 있으며, 이 값은 변경하지 마십시오.

우선 순위 스케줄링의 문제점 중 하나는 'priority inversion'입니다. H, M, L 을 각각 높은, 중간, 낮은 우선 순위의 thread라 해봅시다. H가 L을 기다려야 하고(예를 들면, L이 lock을 걸은 경우) M은 ready list에 있다면, L이 CPU 이용 시간을 얻지 못하기 때문에, H도 절대 CPU를 사용하지 못하게 될 곳입니다. 이 문제의 부분적인 해결책은 L이 lock을 걸고 있는 동안에는 H가 우선순위를 L에게 기부하고, L이 lock을 풀었을 때, H가 기부한 우선순위를 가져오는 것입니다. (Priority Donation)

우선순위 기부를 구현하십시오. 이를 위해선 우선순위 기부가 필요한 모든 상황들을 설명할 수 있어야 합니다. 여러 개의 우선 순위들이 하나의 thread에게 기부되는 Multiple donation을 handle할 수 있어야 합니다. Nested donation 또한 handle할 수 있어야 합니다. 즉, H가 M이 걸은 lock을 기다려야 하고, M이 L이 걸은 lock을 기다려야 하는 경우, M과 L 모두 H의 우선 순위를 갖게 해야 합니다. 필요한 경우, nested donation을 구현할 때, 지나친 nest를 방지하기 위한 8 level 정도의 깊이 제한을 설정할 수 있습니다.

Lock에 대한 priority donation을 구현해야 하지만, 다른 Pintos synchronization donstructs에 대한 priority donation은 구현할 필요가 없습니다. 모든 case의 priority scheduling을 구현해야 합니다.

마지막으로, thread가 자체 우선 순위를 검사하고 수정할 수 있도록 다음 함수들을 구현하십시오. 이 함수들의 Skeleton code는 `threads/thread.c` 에 제공됩니다.

> **void thread_set_priority (int new_priority)**
>
> 현재 thread의 우선 순위를 'new_priority'로 설정합니다. 현재 thread가 더 이상 가장 높은 우선 순위를 가지지 않는 경우 yield 합니다.
>
> **int thread_get_priority (void)**
>
> 현재 thread의 우선순위를 반환합니다. Priority donation이 있는 경우, 기부 받은 우선 순위를 반환합니다.

Thread가 다른 thread의 우선순위를 수정할 수 있도록 하는 인터페이스를 만들 필요는 없습니다. 이 priority scheduler는 이후의 다른 프로젝트에서 사용되지 않습니다.


