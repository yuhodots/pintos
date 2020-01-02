# PintOS Project

UNIST 2019 2학기 Operating System 과목에서 구현하였던 PintOS 프로젝트입니다. 

[Soogeun Park](https://github.com/bwmelon97) 과 교내 Gitlab 저장소에서 공동작업을 진행하였고 프로젝트 1에서 3까지 작업을 완료하였습니다.

따로 작업한 부분도 있고 만나서 같이 작업한 부분도 있어서 코드에 대한 주석을 자세히 달지는 않았습니다.

저희에게는 첫 C 프로젝트여서 이상적인 코드 구현은 아니지만 가끔 보면서 내용을 상기시키고자 업로드하게 되었고

혹시 수업 중에 과제로 핀토스 프로젝트를 하고있다면, 코드가 아닌 메뉴얼 번역만 참고하시길 바랍니다.

주요 용어들은 한국어로 번역하면 이상해지는 경우가 많아서 꼭 영어 원문도 같이 읽으시는 것을 추천드립니다.

아래는 프로젝트에 대한 개요와 Stanford 원문 링크입니다.

### 프로젝트 개요

- Project1 **Threads**
  - timer를 활용하여 thread scheduling 구현하기
  - highest priority scheduling 기법 사용
- Project2 **User Programs**
  - user input arguments를 kernel stack에 쌓기
  - system call handler 구현하기
  - user memory에 접근하여 system call number를 알아내고, 이에 맞는 system call 호출하기
- Porject3 **Virtual Memory**
  - supplemental page table, frame table 만들기
  - 현재의 PintOS를 demanding paging (lazy loading) 방식으로 수정하기
  - page fault handler 구현하기
  - memory-mapped file 구현하기
  - swapping, ~~stack-growth~~ 구현하기

###Stanford 원문

- https://web.stanford.edu/~ouster/cgi-bin/cs140-spring18/pintos/pintos.html