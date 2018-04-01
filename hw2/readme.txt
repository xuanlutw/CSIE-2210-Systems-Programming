/*b05202043 呂佳軒*/

1. Execution

bidding_system
    gcc bidding_system.c -o bidding_system
host
    gcc host.c -o host
player
    gcc player.c -o player
player_bonus
    gcc player_bonus.c -o player_bonus

2. Description

    bidding_system: 用select選擇空閒的host進行下一場
    host: 用pipe跟bidding_system溝通，用FIFO跟player溝通
    player: 依照規則出價
    player_bonus: 當穩贏時出比別人多1元，其他時候出money - random(0, 27)

    其他: Sample的host有問題(1. 1. 1. 2)這種出價會有預期外的結果
          select似乎會傳送EOF給STDIN

3. Self-Examination

    1 用自己的bidding_system + Sample host + Sample player測試過，和Sample結果一樣，花費時間相近
    2 承上，使用select讓host一結束就會被偵測到
    3 承上，fork完才分派工作給host，不會超過host_num的數量
    4 用Sample bidding_system + 自己的 host + Sample player測試過，和Sample結果一樣，花費時間約兩倍，在同一量級內
    5 用Sample bidding_system + Sample host + 自己的 player測試過，和Sample結果一樣，花費時間相近
    6 用自己的 bidding_system + 自己的 host + 自己的 player測試過，和Sample結果一樣，花費時間約兩倍，在同一量級內
    7 測試過make
    另外1 2 3 4 6有把Sample player換成另一個隨機出價的的player，測試bidding_system和host的排名機制
