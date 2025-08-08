_**Komut Bilgi Ekranı:**_

**GETVERSION** : Versiyon bilgisi verilir.

**CPUVER** : İşlemci bilgisi verilir.

**GETTIME** : Anlık zaman bilgisi verilir.

**SETTIME=** : y-m-d H:M:S formatında zaman bilgisi değiştirilir. (Örnek: SETTIME=2001-09-17 14:30:15)

**GETTIMEZONE** : Anlık zaman bölge bilgisi verilir.

**SETTIMEZONE=** : UTC formatında zaman bölge bilgisi değiştirilir. (Örnek: SETTIMEZONE=UTC+1)

**GETSIGNALGROUP** : Işıkların anlık durum bilgisi verilir.

**GETORDER** : Phase ve Sequence modlarındaki ışıkların yanma sıralarını gösterir.

**SETPHASEORDER=** : PHASE modunda ışıkların yanma sırasını değiştirir. (Örnek: SETPHASEORDER=t4-t3-t2-t1)

**SETSEQORDER=** : SEQUENCE modunda ışıkların yanma sırasını değiştirir. (Örnek: SETSEQORDER=t4-t3-t2-t1)

**GETMINSEQTIMEOUT** : Zaman aşımı süre bilgisi verilir.

**SETMINSEQTIMEOUT=** : Zaman aşımı süresi değiştirilir.

**GETMODE** : Aktif mod bilgisi verilir.

**SETMODE=** : Aktif mod SEQUENCE, FLASH ve PHASE arasında değiştirilir. (Örnek: SETMODE=FLASH)

**GETERROR** : Hata bilgisi verilir.

**RESET** : Sistem mevcut modda ve geçerli değişkenlerde yeniden başlatılır.

**CLOSE** : Işıklar kapatılır.

**INFO** : Komut bilgi ekranı açılır.







r1: P8_11 1
y1: P8_12 1
g1: P8_14 0

r2: P8_15 1
y2: P8_16 1
g2: P8_17 0

r3: P8_18 2
y3: P8_26 1
g3: P9_12 1

r4: P9_15 1
y4: P9_23 1
g4: P9_27 3
