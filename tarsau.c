#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define MAX_DOSYA     32
#define MAX_BOYUT     (200 * 1024 * 1024)   /* 200 MB */
#define VARSAYILAN_AD "a.sau"
#define ORG_BOYUT_LEN 10                    /* ilk 10 bayt: ASCII sayısal boyut */


/* İleride doldurulacak fonksiyon prototipleri */
static int  build_archive(const char **dosyalar, int adet, const char *cikti);
static int  extract_archive(const char *arsiv, const char *dizin);

/* ------------------------------------------------------------------ */
/* parse_args: argc/argv'yi ayrıştırır, mod + dosya listesini doldurur */
/* Dönüş: 'b' (arşivle) veya 'a' (çıkar), hata durumunda -1           */
/* ------------------------------------------------------------------ */
static int parse_args(int argc, char *argv[],
                      const char **dosyalar, int *adet,
                      char *cikti,  /* -b için arşiv adı  */
                      char *arsiv,  /* -a için arşiv adı  */
                      char *dizin)  /* -a için hedef dizin */
{
    if (argc < 3) {
        fprintf(stderr, "Kullanim: tarsau -b dosya1 [dosya2 ...] [-o arsiv.sau]\n");
        fprintf(stderr, "          tarsau -a arsiv.sau [dizin]\n");
        return -1;
    }

    /* -b modu: tarsau -b d1 d2 ... [-o cikti.sau] */
    if (strcmp(argv[1], "-b") == 0) {
        strncpy(cikti, VARSAYILAN_AD, PATH_MAX - 1);
        *adet = 0;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                /* -o parametresinden sonra arşiv adı gelmeli */
                if (i + 1 >= argc) {
                    fprintf(stderr, "-o parametresinden sonra dosya adi belirtilmedi!\n");
                    return -1;
                }
                strncpy(cikti, argv[++i], PATH_MAX - 1);
            } else {
                if (*adet >= MAX_DOSYA) {
                    fprintf(stderr, "En fazla %d dosya arsivlenebilir!\n", MAX_DOSYA);
                    return -1;
                }
                dosyalar[(*adet)++] = argv[i];
            }
        }

        if (*adet == 0) {
            fprintf(stderr, "Hic dosya belirtilmedi!\n");
            return -1;
        }
        return 'b';
    }

    /* -a modu: tarsau -a arsiv.sau [dizin] */
    if (strcmp(argv[1], "-a") == 0) {
        strncpy(arsiv, argv[2], PATH_MAX - 1);

        /* İsteğe bağlı hedef dizin; yoksa mevcut dizin kullanılır */
        if (argc >= 4)
            strncpy(dizin, argv[3], PATH_MAX - 1);
        else
            strncpy(dizin, ".", PATH_MAX - 1);

        return 'a';
    }

    fprintf(stderr, "Bilinmeyen parametre: %s\n", argv[1]);
    return -1;
}


/* ------------------------------------------------------------------ */
/* is_ascii_file: dosyanın yalnızca ASCII metin içerip içermediğini   */
/* kontrol eder. Dönüş: 1 = ASCII, 0 = değil                         */
/* ------------------------------------------------------------------ */
static int is_ascii_file(const char *yol)
{
    FILE *f = fopen(yol, "rb");
    if (!f) return 0;

    int c;
    while ((c = fgetc(f)) != EOF) {
        /* Yazdırılabilir ASCII + sekme + satır sonu + CR dışındaki bayt varsa metin değil */
        if (c > 127 || (c < 32 && c != '\t' && c != '\n' && c != '\r')) {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* validate_file: dosyanın varlığını, düz dosya olduğunu,             */
/* ASCII metin olduğunu ve boyut limitini kontrol eder.               */
/* Dönüş: dosya boyutu (>= 0) veya hata durumunda -1                 */
/* ------------------------------------------------------------------ */
static long validate_file(const char *yol, long toplam_boyut)
{
    struct stat sb;

    if (stat(yol, &sb) != 0) {
        fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", yol);
        return -1;
    }

    /* Düz dosya mı? */
    if (!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", yol);
        return -1;
    }

    /* ASCII metin mi? */
    if (!is_ascii_file(yol)) {
        fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", yol);
        return -1;
    }

    /* Toplam boyut 200 MB'ı geçiyor mu? */
    if (toplam_boyut + sb.st_size > MAX_BOYUT) {
        fprintf(stderr, "Toplam dosya boyutu 200 MB sinirini asiyor!\n");
        return -1;
    }

    return (long)sb.st_size;
}

/* ------------------------------------------------------------------ */
/* izin_stringe: stat st_mode değerini "rwxr-xr-x" formatına çevirir */
/* ------------------------------------------------------------------ */
static void izin_stringe(unsigned int mod, char *buf)
{
    buf[0] = (mod & S_IRUSR) ? 'r' : '-';
    buf[1] = (mod & S_IWUSR) ? 'w' : '-';
    buf[2] = (mod & S_IXUSR) ? 'x' : '-';
    buf[3] = (mod & S_IRGRP) ? 'r' : '-';
    buf[4] = (mod & S_IWGRP) ? 'w' : '-';
    buf[5] = (mod & S_IXGRP) ? 'x' : '-';
    buf[6] = (mod & S_IROTH) ? 'r' : '-';
    buf[7] = (mod & S_IWOTH) ? 'w' : '-';
    buf[8] = (mod & S_IXOTH) ? 'x' : '-';
    buf[9] = '\0';
}

/* ------------------------------------------------------------------ */
/* build_archive: .sau dosyasını oluşturur                            */
/* Format: [10 bayt boyut][|ad,izin,boyut|...][içerik1][içerik2]...  */
/* ------------------------------------------------------------------ */
static int build_archive(const char **dosyalar, int adet, const char *cikti)
{
    struct stat sb;
    char izin[10];
    long toplam = 0;

    /* 1. Tüm dosyaları doğrula, boyutları topla */
    for (int i = 0; i < adet; i++) {
        long boyut = validate_file(dosyalar[i], toplam);
        if (boyut < 0)
            return -1;
        toplam += boyut;
    }

    /* 2. Organizasyon bölümünü bellekte oluştur */
    /* Her kayıt en fazla: |255 + 9 + 20 + 2 virgül + 2 boru| ~ 300 bayt */
    char *org = malloc((size_t)adet * 300 + 1);
    if (!org) {
        fprintf(stderr, "Bellek hatasi!\n");
        return -1;
    }
    org[0] = '\0';

    for (int i = 0; i < adet; i++) {
        if (stat(dosyalar[i], &sb) != 0) {
            free(org);
            return -1;
        }
        izin_stringe((unsigned int)sb.st_mode, izin);

        /* Sadece dosya adını kullan, yol bileşenlerini at */
        const char *ad = strrchr(dosyalar[i], '/');
        ad = ad ? ad + 1 : dosyalar[i];

        /* |ad,izinler,boyut| kaydını ekle */
        char kayit[350];
        snprintf(kayit, sizeof(kayit), "|%s,%s,%ld",
                 ad, izin, (long)sb.st_size);
        strcat(org, kayit);
    }
    strcat(org, "|");  /* son kapanış borusu */

    /* 3. Organizasyon bölümü boyutunu hesapla ve 10 baytlık başlığı oluştur */
    /* Başlık (10 bayt) + org string = toplam org bölümü */
    long org_icerik_len = (long)strlen(org);
    long org_toplam     = ORG_BOYUT_LEN + org_icerik_len;

    char baslik[21];
    snprintf(baslik, sizeof(baslik), "%010lld", (long long)org_toplam);

    /* 4. Arşiv dosyasını yaz */
    FILE *cikti_f = fopen(cikti, "wb");
    if (!cikti_f) {
        fprintf(stderr, "%s dosyasi olusturulamadi!\n", cikti);
        free(org);
        return -1;
    }

    /* 10 baytlık boyut başlığını yaz */
    if (fwrite(baslik, 1, ORG_BOYUT_LEN, cikti_f) != (size_t)ORG_BOYUT_LEN) {
        fprintf(stderr, "Yazma hatasi!\n");
        fclose(cikti_f);
        free(org);
        return -1;
    }

    /* Organizasyon kayıtlarını yaz */
    if (fwrite(org, 1, (size_t)org_icerik_len, cikti_f) != (size_t)org_icerik_len) {
        fprintf(stderr, "Yazma hatasi!\n");
        fclose(cikti_f);
        free(org);
        return -1;
    }
    free(org);

    /* 5. Dosya içeriklerini art arda yaz */
    for (int i = 0; i < adet; i++) {
        FILE *giris = fopen(dosyalar[i], "rb");
        if (!giris) {
            fprintf(stderr, "%s acilamadi!\n", dosyalar[i]);
            fclose(cikti_f);
            return -1;
        }

        char tampon[4096];
        size_t okunan;
        while ((okunan = fread(tampon, 1, sizeof(tampon), giris)) > 0) {
            if (fwrite(tampon, 1, okunan, cikti_f) != okunan) {
                fprintf(stderr, "Yazma hatasi!\n");
                fclose(giris);
                fclose(cikti_f);
                return -1;
            }
        }
        fclose(giris);
    }

    fclose(cikti_f);
    return 0;
}
/* ------------------------------------------------------------------ */
/* extract_archive: ileride Aşama 4'te doldurulacak                   */
/* ------------------------------------------------------------------ */
static int extract_archive(const char *arsiv, const char *dizin)
{
    (void)arsiv; (void)dizin;
    fprintf(stderr, "extract_archive: henuz implemente edilmedi\n");
    return -1;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    const char *dosyalar[MAX_DOSYA];
    int   adet  = 0;
    char  cikti[PATH_MAX] = {0};
    char  arsiv[PATH_MAX] = {0};
    char  dizin[PATH_MAX] = {0};

    int mod = parse_args(argc, argv, dosyalar, &adet, cikti, arsiv, dizin);
    if (mod < 0)
        return 1;

    if (mod == 'b') {
        if (build_archive(dosyalar, adet, cikti) != 0)
            return 1;
        printf("Dosyalar birlestirildi.\n");
    } else {
        if (extract_archive(arsiv, dizin) != 0)
            return 1;
        printf("%s dizininde dosyalar acildi.\n", dizin);
    }

    return 0;
}

