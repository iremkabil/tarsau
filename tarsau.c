
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
/* build_archive: ileride Aşama 3'te doldurulacak                     */
/* ------------------------------------------------------------------ */
static int build_archive(const char **dosyalar, int adet, const char *cikti)
{
    long toplam = 0;

    /* Önce tüm dosyaları doğrula */
    for (int i = 0; i < adet; i++) {
        long boyut = validate_file(dosyalar[i], toplam);
        if (boyut < 0)
            return -1;
        toplam += boyut;
    }

    (void)cikti;  /* Aşama 3'te kullanılacak */
    fprintf(stderr, "Dogrulama tamam. Toplam: %ld bayt\n", toplam);
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

