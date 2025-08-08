#include <gpiod.h>      // libgpiod library
#include <iostream>     // for screen messages
#include <string>
#include <unistd.h>     // for sleep()
#include <algorithm>
#include <thread>       // for synchronous functions
#include <chrono>       // for milliseconds()
#include <atomic>       // for atomic variables in threads (Aynı anda bir thread okurken başka biri güvenli şekilde yazabilir.)
#include <random>       // for phase mode green times
#include <ctime>        // for time()
#include <iomanip>      // giriş/çıkış (I/O) biçimlendirme işlemleri
#include <sstream>      // string tabanlı giriş/çıkış işlemleri
#include <fstream>      // Dosyadan veri okumak-yazmak

std::atomic<bool> inSeq(false);
std::atomic<bool> inFlash(false);
std::atomic<bool> inPhase(false);
std::thread seqThread;
std::thread flashThread;
std::thread phaseThread;
std::vector<int> phaseOrder = {0, 1, 2, 3};     // Phase modu için ilk faz sırası (t1-t2-t3-t4)
std::vector<int> sequenceOrder = {0, 1, 2, 3};
std::atomic<bool> runTimeoutWatcher(false);     // Zaman aşımı denetleyicisi
std::thread timeoutWatcherThread;
std::atomic<std::chrono::steady_clock::time_point> lastCommandTime;     // std::chrono::steady_clock::time_point tanımı şuanki zamanı tutar

struct TrafficLight {
    gpiod_line *red;        // TrafficLight t1 {r1, y1, g1};
    gpiod_line *yellow;
    gpiod_line *green;

    int red_val = 0;
    int yellow_val = 0;
    int green_val = 0;

    void set(int r, int y, int g) {
        red_val = r;
        yellow_val = y;
        green_val = g;
        gpiod_line_set_value(red, r);
        gpiod_line_set_value(yellow, y);
        gpiod_line_set_value(green, g);
    }

    std::string get_signal_group() {
        if (red_val == 1 && yellow_val == 1) return "RED+YELLOW";
        if (green_val == 1) return "GREEN";
        if (yellow_val == 1) return "YELLOW";
        if (red_val == 1) return "RED";
        return "OFF";
    }
};

void smart_sleep(int seconds, std::atomic<bool>&running) {
    for (int i = 0; i < seconds * 10; ++i) {
        if (!running) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void phase_mode(std::vector<TrafficLight>&lights, std::atomic<bool>&running) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(3, 10);

    while (running) {
        for (int i = 0; i < phaseOrder.size(); ++i) {
            int currled = phaseOrder[i];
            int nextled = phaseOrder[(i + 1) % phaseOrder.size()];
            for (int j = 0; j < lights.size(); ++j) {
                lights[j].set(1, 0, 0);
            }
            lights[currled].set(0, 0, 1);
            smart_sleep(dist(gen), running);
            lights[currled].set(0, 1, 0);
            smart_sleep(2, running);
            lights[currled].set(1, 0, 0);
            lights[nextled].set(1, 1, 0);
            smart_sleep(2, running);
        }
    }
}

void flash_mode(std::vector<TrafficLight>&lights, std::atomic<bool>&running) {
    while (running) {
        for (int i = 0; i < lights.size(); ++i) {
            lights[i].set(0, 1, 0);
        }
        smart_sleep(1, running);
        for (int i = 0; i < lights.size(); ++i) {
            lights[i].set(0, 0, 0);
        }
        smart_sleep(1, running);
    }
}

void sequence_mode(std::vector<TrafficLight>&lights, std::atomic<bool>&running) {
    while (running) {
        for (int i = 0; i < sequenceOrder.size(); ++i) {
            if (!running) return;
            int currled = sequenceOrder[i];
            int nextled = sequenceOrder[(i + 1) % sequenceOrder.size()];
            for (int j = 0; j < lights.size(); ++j) {
                if (j == currled)
                    lights[j].set(0, 0, 1); // Sıradaki ışık yeşil
                else
                    lights[j].set(1, 0, 0); // Diğerleri kırmızı
            }
            smart_sleep(5, running);
            if (!running) return;
            lights[currled].set(0, 1, 0);
            smart_sleep(2, running);
            if (!running) return;
            lights[currled].set(1, 0, 0);
            lights[nextled].set(1, 1, 0);
            smart_sleep(2, running);
            if (!running) return;
        }
    }
}

void reset_lights(std::vector<TrafficLight>&lights) {
    for (int i = 0; i < lights.size(); ++i) {
        lights[i].set(0, 0, 0);
    }
}

void timeout_watcher(std::vector<TrafficLight>&lights, std::atomic<bool>&running, std::string &minseqtimeout_ref, std::string &activemode_ref) {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (inPhase) {
            auto now = std::chrono::steady_clock::now();
            auto last_cmd_time = lastCommandTime.load();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - last_cmd_time).count();
            int timeout_duration = std::stoi(minseqtimeout_ref);        // string to int
            if (elapsed_seconds >= timeout_duration) {
                std::cout << "Zaman aşımı! " << timeout_duration << " saniyedir komut gelmedi. PHASE modundan SEQUENCE moduna geçiliyor...\n";
                std::cout << "Komut giriniz: " << std::flush;       // yazılan veriyi hemen ekrana basar
                inPhase = false;
                if (phaseThread.joinable()) {
                    phaseThread.join();
                }
                activemode_ref = "SEQUENCE";
                reset_lights(lights);
                inSeq = true;
                seqThread = std::thread(sequence_mode, std::ref(lights), std::ref(inSeq));
                lastCommandTime = std::chrono::steady_clock::now();     // Kronometreyi sıfırla
            }
        }
    }
}

void start_mode(const std::string &activemode, std::vector<TrafficLight> &lights) {
    if (activemode == "SEQUENCE") {
        inSeq = true;
        seqThread = std::thread(sequence_mode, std::ref(lights), std::ref(inSeq));
        std::cout << "Sistem SEQUENCE modunda başlatıldı.\n";
    } else if (activemode == "FLASH") {
        inFlash = true;
        flashThread = std::thread(flash_mode, std::ref(lights), std::ref(inFlash));
        std::cout << "Sistem FLASH modunda başlatıldı.\n";
    } else if (activemode == "PHASE") {
        inPhase = true;
        phaseThread = std::thread(phase_mode, std::ref(lights), std::ref(inPhase));
        std::cout << "Sistem PHASE modunda başlatıldı.\n";
    }
}

std::string format_tm(const std::tm& t) {       //time değeri için sadece okunuyor ve bu içerikten yeni bir string üretiliyor.
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &t);
    return std::string(time_buf);
}

std::string format_order_vector(const std::vector<int>& order) {        // GETORDER komutu için ışık sıralarından string oluşturur.
    std::stringstream ss;
    for (size_t i = 0; i < order.size(); ++i) {
        ss << "t" << (order[i] + 1);
        if (i < order.size() - 1) {
            ss << "-";
        }
    }
    return ss.str();
}

int main() {

    int r1_pin = 13;           // P8_11 tanımlama
    int y1_pin = 12;           // P8_12 tanımlama
    int g1_pin = 26;           // P8_14 tanımlama

    int r2_pin = 15;           // P8_15 tanımlama
    int y2_pin = 14;           // P8_16 tanımlama
    int g2_pin = 27;           // P8_17 tanımlama
  
    int r3_pin = 1;            // P8_18 tanımlama
    int y3_pin = 29;           // P8_26 tanımlama
    int g3_pin = 28;           // P9_12 tanımlama
 
    int r4_pin = 16;           // P9_15 tanımlama
    int y4_pin = 17;           // P9_23 tanımlama
    int g4_pin = 19;           // P9_27 tanımlama

    int status_led_pin = 18; // P9_14 durum LED'i pini

    gpiod_chip *chip0 = gpiod_chip_open_by_name("gpiochip0");
    gpiod_chip *chip1 = gpiod_chip_open_by_name("gpiochip1");
    gpiod_chip *chip2 = gpiod_chip_open_by_name("gpiochip2");
    gpiod_chip *chip3 = gpiod_chip_open_by_name("gpiochip3");

    gpiod_line *r1 = gpiod_chip_get_line(chip1, r1_pin);
    gpiod_line_request_output(r1, "led_control", 0);    
    gpiod_line *y1 = gpiod_chip_get_line(chip1, y1_pin);
    gpiod_line_request_output(y1, "led_control", 0);
    gpiod_line *g1 = gpiod_chip_get_line(chip0, g1_pin);
    gpiod_line_request_output(g1, "led_control", 0);

    gpiod_line *r2 = gpiod_chip_get_line(chip1, r2_pin);
    gpiod_line_request_output(r2, "led_control", 0);    
    gpiod_line *y2 = gpiod_chip_get_line(chip1, y2_pin);
    gpiod_line_request_output(y2, "led_control", 0);
    gpiod_line *g2 = gpiod_chip_get_line(chip0, g2_pin);
    gpiod_line_request_output(g2, "led_control", 0);

    gpiod_line *r3 = gpiod_chip_get_line(chip2, r3_pin);
    gpiod_line_request_output(r3, "led_control", 0);    
    gpiod_line *y3 = gpiod_chip_get_line(chip1, y3_pin);
    gpiod_line_request_output(y3, "led_control", 0);
    gpiod_line *g3 = gpiod_chip_get_line(chip1, g3_pin);
    gpiod_line_request_output(g3, "led_control", 0);

    gpiod_line *r4 = gpiod_chip_get_line(chip1, r4_pin);
    gpiod_line_request_output(r4, "led_control", 0);    
    gpiod_line *y4 = gpiod_chip_get_line(chip1, y4_pin);
    gpiod_line_request_output(y4, "led_control", 0);
    gpiod_line *g4 = gpiod_chip_get_line(chip3, g4_pin);
    gpiod_line_request_output(g4, "led_control", 0);

    gpiod_line *status_led = gpiod_chip_get_line(chip1, status_led_pin);
    gpiod_line_request_output(status_led, "status_led", 0);

    std::string timezone = "UTC+3";
    bool customTimeSet = false;                      // Kullanıcı SETTIME komutu verdi mi?
    std::tm custom_tm = {};                          // Kullanıcının girdiği zaman (tm yapısında)
    std::chrono::steady_clock::time_point setPoint;  // SETTIME anındaki steady_clock zamanı
    std::string minseqtimeout = "40";
    std::string activemode = "SEQUENCE";
    const std::string initial_mode = activemode;        // RESET komutu için başlangıç modunu saklarız.

    TrafficLight t1 {r1, y1, g1};       // t1.red = r1, t1.yellow = y1, t1.green = g1
    TrafficLight t2 {r2, y2, g2};
    TrafficLight t3 {r3, y3, g3};
    TrafficLight t4 {r4, y4, g4};
    std::vector<TrafficLight> lights = {t1, t2, t3, t4};

    lastCommandTime = std::chrono::steady_clock::now();         // kronometreyi başlat
    runTimeoutWatcher = true;
    timeoutWatcherThread = std::thread(timeout_watcher, std::ref(lights), std::ref(runTimeoutWatcher), std::ref(minseqtimeout), std::ref(activemode));
    std::cout << "Program başlatılıyor...\n";
    std::cout << "Komut bilgi ekranı için INFO komutunu veriniz.\n";
    start_mode(activemode, lights);
    gpiod_line_set_value(status_led, 1);

    while (true) {

        std::string komut;
        std::cout << "Komut giriniz: ";
        std::getline(std::cin, komut);
        lastCommandTime = std::chrono::steady_clock::now();


        size_t pos;                                                     // unsigned integer
        while ((pos = komut.find("\\r")) != std::string::npos) {        // "\\r" bulunuyorsa
            komut.replace(pos, 2, "\r");
        }

        komut.erase(std::remove(komut.begin(), komut.end(), '\r'), komut.end());

        if (komut == "GETVERSION") {
            std::cout << "VERSION=2.5\n";
        }

        else if (komut == "GETTIME") {
            if (customTimeSet) {        // SETTIME ile kullanıcı zaman girdiyse
            auto now = std::chrono::steady_clock::now();        // güncel zaman
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - setPoint).count();      // SET komutu sonrası geçen zaman bulunur
            std::tm current_tm = custom_tm;
            time_t custom_time_t = std::mktime(&current_tm);
            custom_time_t += elapsed;                         // Kullanıcı saatine geçen süreyi ekle
            std::tm updated_tm;
            localtime_r(&custom_time_t, &updated_tm);
            std::cout << "TIME=" << format_tm(updated_tm) << "\n";
            } else {
                time_t now = std::time(nullptr);        // direkt sistem saati alınır
                std::tm system_tm;
                localtime_r(&now, &system_tm);
                std::cout << "TIME=" << format_tm(system_tm) << "\n";
              }
        }

        else if (komut.rfind("SETTIME=", 0) == 0) {
            std::string ftime = komut.substr(8);
            std::tm t = {};
            strptime(ftime.c_str(), "%Y-%m-%d %H:%M:%S", &t);
            custom_tm = t;
            customTimeSet = true;       // kullanıcının SETTIME ile yeni zaman değeri girdiğini gösterir
            setPoint = std::chrono::steady_clock::now();
            std::cout << "Zaman güncellendi: " << ftime << "\n";
        }

        else if (komut == "GETTIMEZONE") {
            std::ifstream tzfile("/etc/timezone");
            std::string tz;
            std::getline(tzfile, tz);
            std::cout << "TIMEZONE=" << tz << "\n";
        }

        else if (komut.rfind("SETTIMEZONE=", 0) == 0) {
            std::string new_tz = komut.substr(12);
            std::ofstream tzfile("/etc/timezone");
            tzfile << new_tz << "\n";
            tzfile.close();
            setenv("TZ", new_tz.c_str(), 1);
            tzset();
            std::cout << "Zaman dilimi güncellendi: " << new_tz << "\n";
        }
        
        else if (komut == "CPUVER") {
            std::cout << "CPUVER=AM3358BZC\n";
        }

        else if (komut == "GETSIGNALGROUP") {
            std::string sonuc = "";
            for (int i = 0; i < lights.size(); ++i) {
                sonuc += "t" + std::to_string(i + 1) + ":" + lights[i].get_signal_group();
                if (i != lights.size() - 1) {
                    sonuc += ", ";
                }
            }
            std::cout << sonuc << "\n";
        }

        else if (komut.rfind("SETPHASEORDER=", 0) == 0) {
            std::string siraliKomut = komut.substr(14);     // "t1-t2-t3-t4" kısmını al
            std::stringstream ss(siraliKomut);
            std::string parca;
            std::vector<int> yeniSira;
            bool kullanilanYonler[4] = {false};
            while (std::getline(ss, parca, '-')) {
                if (parca.length() == 2 && parca[0] == 't') {       // Her bir parçayı ("t1", "t2" vb.) kontrol et
                    char yonNo = parca[1];
                    if (yonNo >= '1' && yonNo <= '4') {
                        int index = yonNo - '1';
                        if (!kullanilanYonler[index]) {
                            yeniSira.push_back(index);
                            kullanilanYonler[index] = true;
                        } else {        // Hata: Aynı yön birden fazla kullanılmış
                            yeniSira.clear();       // Hatalı durumu belirtmek için listeyi temizle
                            break;
                        }
                    } else {        // Hata: 't5' gibi geçersiz bir yön
                        yeniSira.clear();
                        break;
                    }
                } else {        // Hata: "t1" formatına uymayan bir parça
                    yeniSira.clear();
                    break;
                }
            }
            if (yeniSira.size() == 4) {
                phaseOrder = yeniSira;
                std::cout << "Phase sırası güncellendi: " << siraliKomut << "\n";
                if (inPhase) {
                    inPhase = false;
                    if (phaseThread.joinable()) {
                        phaseThread.join();
                    }
                    reset_lights(lights);
                    inPhase = true;
                    phaseThread = std::thread(phase_mode, std::ref(lights), std::ref(inPhase));
                    std::cout << "PHASE modu yeni sırayla yeniden başlatıldı.\n";
                } else {
                    std::cout << "Uyarı: Sistem PHASE modunda değil. Sıra kaydedildi ama şu anda uygulanamadı.\n";
                }
            } else {
                std::cout << "Hatalı komut! Doğru format: SETPHASEORDER=t1-t2-t3-t4 (4 farklı ve geçerli yön kullanın)\n";
            }
        }

        else if (komut.rfind("SETSEQORDER=", 0) == 0) {
            std::string siraliKomut = komut.substr(12);
            std::stringstream ss(siraliKomut);
            std::string parca;
            std::vector<int> yeniSira;
            bool kullanilanYonler[4] = {false};
            while (std::getline(ss, parca, '-')) {
                if (parca.length() == 2 && parca[0] == 't') {
                    char yonNo = parca[1];
                    if (yonNo >= '1' && yonNo <= '4') {
                        int index = yonNo - '1';
                        if (!kullanilanYonler[index]) {
                            yeniSira.push_back(index);
                            kullanilanYonler[index] = true;
                        } else {
                            yeniSira.clear();
                            break;
                        }
                    } else {
                        yeniSira.clear();
                        break;
                    }
                } else {
                    yeniSira.clear();
                    break;
                }
            }
            if (yeniSira.size() == 4) {
                sequenceOrder = yeniSira;
                std::cout << "Sequence sırası güncellendi: " << siraliKomut << "\n";
                if (inSeq) {
                    inSeq = false;
                    if (seqThread.joinable()) {
                        seqThread.join();
                    }
                    reset_lights(lights);
                    inSeq = true;
                    seqThread = std::thread(sequence_mode, std::ref(lights), std::ref(inSeq));
                    std::cout << "SEQUENCE modu yeni sırayla yeniden başlatıldı.\n";
                } else {
                    std::cout << "Uyarı: Sistem SEQUENCE modunda değil. Sıra kaydedildi ama şu anda uygulanamadı.\n";
                }
            } else {
                std::cout << "Hatalı komut! Format: t1-t2-t3-t4 (4 farklı ve geçerli yön kullanın)\n";
            }
        }

        else if (komut == "GETORDER") {
            std::string phase_str = format_order_vector(phaseOrder);
            std::string seq_str = format_order_vector(sequenceOrder);
            std::cout << "Phase mode sirasi: " << phase_str << "\n";
            std::cout << "Sequence mode sirasi: " << seq_str << "\n";
        }

        else if (komut == "GETMINSEQTIMEOUT") {
            std::cout << "MINSEQTIMEOUT=" << minseqtimeout << " saniye\n";
        }

        else if (komut.rfind("SETMINSEQTIMEOUT=", 0) == 0) {
            minseqtimeout = komut.substr(17);
            std::cout << "Minumum zaman aşımı süresi güncellendi: " << minseqtimeout << " saniye\n";
        }

        else if (komut == "GETMODE") {
            std::cout << "ACTIVEMOD=" << activemode << "\n";
        }

        else if (komut.rfind("SETMODE=", 0) == 0) {
            activemode = komut.substr(8);
            std::cout << "Çalışma modu güncellendi: " << activemode << "\n";

            if (inSeq) {        // önceki durumlar temizlenir
                inSeq = false;
                if (seqThread.joinable()) seqThread.join();
                std::cout << "SEQUENCE durduruldu.\n";
            }

            if (inFlash) {
                inFlash = false;
                if (flashThread.joinable()) flashThread.join();
                std::cout << "FLASH durduruldu.\n";
            }

            if (inPhase) {
                inPhase = false;
                if (phaseThread.joinable()) phaseThread.join();
                std::cout << "PHASE durduruldu.\n";
            }
            reset_lights(lights);

            if (activemode == "SEQUENCE") {
                inSeq = true;
                seqThread = std::thread(sequence_mode, std::ref(lights), std::ref(inSeq));
                std::cout << "SEQUENCE mode başlatıldı.\n";
            }

            else if (activemode == "FLASH") {
                inFlash = true;
                flashThread = std::thread(flash_mode, std::ref(lights), std::ref(inFlash));
                std::cout << "FLASH mode başlatıldı.\n";
            }

            else if (activemode == "PHASE") {
                inPhase = true;
                phaseThread = std::thread(phase_mode, std::ref(lights), std::ref(inPhase));
                std::cout << "PHASE mode başlatıldı.\n";
            }
        }     

        else if (komut == "GETERROR") {
            std::cout << "ERROR=1\n";
        }

        else if (komut == "INFO") {
            std::cout << "\n--- Komut Bilgi Ekranı ---\n";
            std::cout << "GETVERSION\t\t: Versiyon bilgisi verilir.\n";
            std::cout << "CPUVER\t\t\t: İşlemci bilgisi verilir.\n";
            std::cout << "GETTIME\t\t\t: Anlık zaman bilgisi verilir.\n";
            std::cout << "SETTIME=\t\t: y-m-d H:M:S formatında zaman bilgisi değiştirilir. (Örnek: SETTIME=2001-09-17 14:30:15)\n";
            std::cout << "GETTIMEZONE\t\t: Anlık zaman bölge bilgisi verilir.\n";
            std::cout << "SETTIMEZONE=\t\t: UTC formatında zaman bölge bilgisi değiştirilir. (Örnek: SETTIMEZONE=UTC+1)\n";
            std::cout << "GETSIGNALGROUP\t\t: Işıkların anlık durum bilgisi verilir.\n";
            std::cout << "GETORDER\t\t: Phase ve Sequence modlarındaki ışıkların yanma sıralarını gösterir.\n";
            std::cout << "SETPHASEORDER=\t\t: PHASE modunda ışıkların yanma sırasını değiştirir. (Örnek: SETPHASEORDER=t4-t3-t2-t1)\n";
            std::cout << "SETSEQORDER=\t\t: SEQUENCE modunda ışıkların yanma sırasını değiştirir. (Örnek: SETSEQORDER=t4-t3-t2-t1)\n";
            std::cout << "GETMINSEQTIMEOUT\t: Zaman aşımı süre bilgisi verilir.\n";
            std::cout << "SETMINSEQTIMEOUT=\t: Zaman aşımı süresi değiştirilir.\n";
            std::cout << "GETMODE\t\t\t: Aktif mod bilgisi verilir.\n";
            std::cout << "SETMODE=\t\t: Aktif mod SEQUENCE, FLASH ve PHASE arasında değiştirilir. (Örnek: SETMODE=FLASH)\n";
            std::cout << "GETERROR\t\t: Hata bilgisi verilir.\n";
            std::cout << "RESET\t\t\t: Sistem başlangıç modunda ve geçerli değişkenlerde yeniden başlatılır.\n";
            std::cout << "CLOSE\t\t\t: Işıklar kapatılır.\n";
            std::cout << "INFO\t\t\t: Komut bilgi ekranı açılır.\n";
            std::cout << "---------------------------\n";
        }

        else if (komut == "RESET") {
            std::cout << "Sistem sıfırlanıyor ve başlangıç moduna dönülüyor...\n";
            inPhase = false;
            inSeq = false;
            inFlash = false;
            if (phaseThread.joinable()) phaseThread.join();
            if (seqThread.joinable()) seqThread.join();
            if (flashThread.joinable()) flashThread.join();
            reset_lights(lights);
            activemode = initial_mode;
            std::cout << "Aktif mod, başlangıç modu olan '" << activemode << "' olarak ayarlandı.\n";
            if (activemode == "PHASE") {
                inPhase = true;
                phaseThread = std::thread(phase_mode, std::ref(lights), std::ref(inPhase));
                std::cout << "Sistem PHASE modunda yeniden başlatıldı.\n";
            } else if (activemode == "SEQUENCE") {
                inSeq = true;
                seqThread = std::thread(sequence_mode, std::ref(lights), std::ref(inSeq));
                std::cout << "Sistem SEQUENCE modunda yeniden başlatıldı.\n";
            } else if (activemode == "FLASH") {
                inFlash = true;
                flashThread = std::thread(flash_mode, std::ref(lights), std::ref(inFlash));
                std::cout << "Sistem FLASH modunda yeniden başlatıldı.\n";
            } else {
                std::cout << "Bilinmeyen başlangıç modu: " << activemode << "\n";
            }
        }

        else if (komut == "CLOSE") {
        std::cout << "Tüm ışıklar kapatılıyor...\n";        
        std::cout << "RESET komutu ile sistemi yeniden başlatabilirsiniz.\n";

            if (inSeq || inFlash || inPhase) {
                inSeq = false;
                inFlash = false;
                inPhase = false;
                if (seqThread.joinable()) seqThread.join();
                if (flashThread.joinable()) flashThread.join();
                if (phaseThread.joinable()) phaseThread.join();
            }
            reset_lights(lights);
        }

        else {
            std::cout << "Bilinmeyen komut!\n";
        }

    }

    return 0;
}