Trafik Kontrol Sistemi Protokol Komutları


	1. Versiyon Bilgisi: GETVERSION\r
	   Cevap: "VERSION=2.5"



	2. Tarih-Zaman Bilgisi: GETTIME\r 
	   Cevap: "TIME=10.07.2025 14:30:00\r"

	   SETTIME{yyyy-mm-dd hh:mm:ss}\r
	   Cevap: "SETTIME{2025-07-10 14:30:00}\r"



	3. Time Zone: GETTIMEZONE\r
       Cevap: "TIMEZONE=UTC+3\r"

	   SETTIMEZONE{timezone}\r
 	   Cevap: "SETTIMEZONE{UTC+3}\r"



	4. Çalışma Modu: GETMODE\r  
  	   Cevap: "MODE=PHASE\r", "MODE=SEQUENCE\r", "MODE=FLASH\r"

	   SETMODE{mode}\r  
	   Cevap: "SETMODE{PHASE}\r"



	5. Sinyal Grup Bilgisi: GETSIGNALGROUP\r
	   Cevap: " "



	6. Faz Değiştirme: CHANGEPHASE{phase_time}\r  
  	   Cevap: "CHANGEPHASE{20}\r"



	7. Min Sequence Timeout Bilgisi Alma: GETMINSEQTIMEOUT\r
	   Cevap: "TIMEOUT=20\r"

	   SETMINSEQTIMEOUT{timeout}\r
	   Cevap: "SETMINSEQTIMEOUT{timeout}\r"



	8. Hata Bilgisi: GETERROR\r
 	   Cevap: "ERROR=1\r"


	
	9. Versiyon Bilgisi: CPUVER\r
	   Cevap: "CPUVER=AM3358BZC\r"
