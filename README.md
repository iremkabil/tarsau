# tarsau

Linux'ta C99 ile yazılmış, sıkıştırma yapmayan arşivleme aracı.

## Derleme

```bash
make
```

## Kullanım

**Arşivleme:**
```bash
./tarsau -b dosya1.txt dosya2.txt
./tarsau -b dosya1.txt dosya2.txt arsiv.sau
```

**Çıkarma:**
```bash
./tarsau -a arsiv.sau
./tarsau -a arsiv.sau hedef_dizin/
```

Çıktı dosyası belirtilmezse varsayılan ad `a.sau` kullanılır.

## .sau Dosya Formatı
[10 bayt: organizasyon boyutu]|dosyaadi,izinler,boyut|dosyaadi,izinler,boyut|...[içerikler]

- İlk 10 bayt: organizasyon bölümünün boyutu (ASCII, sıfır dolgulu, örn. `0000000087`)
- Her dosya `|dosyaadi,izinler,boyut|` kaydıyla temsil edilir
- Organizasyon bölümünden hemen sonra dosya içerikleri ard arda gelir, ayraç yoktur

## Kısıtlamalar

- Yalnızca ASCII metin dosyaları desteklenir
- Maksimum toplam boyut: 200 MB
- Maksimum dosya sayısı: 32

## Hata Mesajları

| Durum | Mesaj |
|---|---|
| Binary/ASCII dışı dosya | `t7 giriş dosyasının formatı uyumsuzdur!` |
| Bozuk veya geçersiz arşiv | `Arşiv dosyası uygunsuz veya bozuk!` |

## Dosya Yapısı
tarsau/
├── tarsau.c
└── Makefile
