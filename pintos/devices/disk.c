#include "devices/disk.h"
#include <ctype.h>
#include <debug.h>
#include <stdbool.h>
#include <stdio.h>
#include "devices/timer.h"
#include "threads/io.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

/* The code in this file is an interface to an ATA (IDE)
   controller.  It attempts to comply to [ATA-3]. */
/* [한국어 번역]
 * 이 파일의 코드는 ATA(IDE) 컨트롤러에 대한 인터페이스다.
 * [ATA-3] 규격을 준수하도록 작성되었다.
 */

/* ATA command block port addresses. */
/* [한국어 번역] ATA 명령 블록의 포트 주소. */
#define reg_data(CHANNEL) ((CHANNEL)->reg_base + 0)   /* Data. */
/* [한국어 번역] 데이터. */
#define reg_error(CHANNEL) ((CHANNEL)->reg_base + 1)  /* Error. */
/* [한국어 번역] 오류. */
#define reg_nsect(CHANNEL) ((CHANNEL)->reg_base + 2)  /* Sector Count. */
/* [한국어 번역] 섹터 수. */
#define reg_lbal(CHANNEL) ((CHANNEL)->reg_base + 3)   /* LBA 0:7. */
/* [한국어 번역] LBA의 0~7번 비트. */
#define reg_lbam(CHANNEL) ((CHANNEL)->reg_base + 4)   /* LBA 15:8. */
/* [한국어 번역] LBA의 15~8번 비트. */
#define reg_lbah(CHANNEL) ((CHANNEL)->reg_base + 5)   /* LBA 23:16. */
/* [한국어 번역] LBA의 23~16번 비트. */
#define reg_device(CHANNEL) ((CHANNEL)->reg_base + 6) /* Device/LBA 27:24. */
/* [한국어 번역] 장치 및 LBA의 27~24번 비트. */
#define reg_status(CHANNEL) ((CHANNEL)->reg_base + 7) /* Status (r/o). */
/* [한국어 번역] 상태. 읽기 전용. */
#define reg_command(CHANNEL) reg_status(CHANNEL)      /* Command (w/o). */
/* [한국어 번역] 명령. 쓰기 전용. */

/* ATA control block port addresses.
   (If we supported non-legacy ATA controllers this would not be
   flexible enough, but it's fine for what we do.) */
/* [한국어 번역]
 * ATA 제어 블록의 포트 주소.
 * 레거시 방식이 아닌 ATA 컨트롤러를 지원하기에는 유연성이 부족하지만,
 * 현재 용도에는 충분하다.
 */
#define reg_ctl(CHANNEL) ((CHANNEL)->reg_base + 0x206) /* Control (w/o). */
/* [한국어 번역] 제어. 쓰기 전용. */
#define reg_alt_status(CHANNEL) reg_ctl(CHANNEL)       /* Alt Status (r/o). */
/* [한국어 번역] 대체 상태. 읽기 전용. */

/* Alternate Status Register bits. */
/* [한국어 번역] 대체 상태 레지스터의 비트. */
#define STA_BSY 0x80  /* Busy. */
/* [한국어 번역] 사용 중. */
#define STA_DRDY 0x40 /* Device Ready. */
/* [한국어 번역] 장치 준비 완료. */
#define STA_DRQ 0x08  /* Data Request. */
/* [한국어 번역] 데이터 요청. */

/* Control Register bits. */
/* [한국어 번역] 제어 레지스터의 비트. */
#define CTL_SRST 0x04 /* Software Reset. */
/* [한국어 번역] 소프트웨어 리셋. */

/* Device Register bits. */
/* [한국어 번역] 장치 레지스터의 비트. */
#define DEV_MBS 0xa0 /* Must be set. */
/* [한국어 번역] 반드시 설정해야 한다. */
#define DEV_LBA 0x40 /* Linear based addressing. */
/* [한국어 번역] 선형 기반 주소 지정. */
#define DEV_DEV 0x10 /* Select device: 0=master, 1=slave. */
/* [한국어 번역] 장치 선택: 0은 마스터, 1은 슬레이브. */

/* Commands.
   Many more are defined but this is the small subset that we
   use. */
/* [한국어 번역]
 * 명령들.
 * 이 밖에도 많은 명령이 정의되어 있지만 여기서는 이 일부만 사용한다.
 */
#define CMD_IDENTIFY_DEVICE 0xec    /* IDENTIFY DEVICE. */
/* [한국어 번역] 장치 식별. */
#define CMD_READ_SECTOR_RETRY 0x20  /* READ SECTOR with retries. */
/* [한국어 번역] 재시도를 포함한 섹터 읽기. */
#define CMD_WRITE_SECTOR_RETRY 0x30 /* WRITE SECTOR with retries. */
/* [한국어 번역] 재시도를 포함한 섹터 쓰기. */

/* An ATA device. */
/* [한국어 번역] ATA 장치. */
struct disk {
    char name[8];            /* Name, e.g. "hd0:1". */
    /* [한국어 번역] 이름. 예: "hd0:1". */
    struct channel *channel; /* Channel disk is on. */
    /* [한국어 번역] 디스크가 연결된 채널. */
    int dev_no;              /* Device 0 or 1 for master or slave. */
    /* [한국어 번역] 마스터 또는 슬레이브에 해당하는 장치 번호 0 또는 1. */

    bool is_ata;            /* 1=This device is an ATA disk. */
    /* [한국어 번역] 1이면 이 장치는 ATA 디스크다. */
    disk_sector_t capacity; /* Capacity in sectors (if is_ata). */
    /* [한국어 번역] 섹터 단위의 용량. is_ata인 경우에 해당한다. */

    long long read_cnt;  /* Number of sectors read. */
    /* [한국어 번역] 읽은 섹터 수. */
    long long write_cnt; /* Number of sectors written. */
    /* [한국어 번역] 기록한 섹터 수. */
};

/* An ATA channel (aka controller).
   Each channel can control up to two disks. */
/* [한국어 번역]
 * ATA 채널. 컨트롤러라고도 한다.
 * 각 채널은 최대 두 개의 디스크를 제어할 수 있다.
 */
struct channel {
    char name[8];      /* Name, e.g. "hd0". */
    /* [한국어 번역] 이름. 예: "hd0". */
    uint16_t reg_base; /* Base I/O port. */
    /* [한국어 번역] 기준 I/O 포트. */
    uint8_t irq;       /* Interrupt in use. */
    /* [한국어 번역] 사용하는 인터럽트. */

    struct lock lock;         /* Must acquire to access the controller. */
    /* [한국어 번역] 컨트롤러에 접근하려면 반드시 이 락을 획득해야 한다. */
    bool expecting_interrupt; /* True if an interrupt is expected, false if
								   any interrupt would be spurious. */
    /* [한국어 번역]
     * 인터럽트를 기다리는 상태면 true다.
     * false이면 발생하는 모든 인터럽트가 불필요한 인터럽트다.
     */
    struct semaphore completion_wait; /* Up'd by interrupt handler. */
    /* [한국어 번역] 인터럽트 핸들러가 세마포어를 올린다. */

    struct disk devices[2]; /* The devices on this channel. */
    /* [한국어 번역] 이 채널에 연결된 장치들. */
};

/* We support the two "legacy" ATA channels found in a standard PC. */
/* [한국어 번역] 일반적인 PC에 있는 두 개의 "레거시" ATA 채널을 지원한다. */
#define CHANNEL_CNT 2
static struct channel channels[CHANNEL_CNT];

static void reset_channel(struct channel *);
static bool check_device_type(struct disk *);
static void identify_ata_device(struct disk *);

static void select_sector(struct disk *, disk_sector_t);
static void issue_pio_command(struct channel *, uint8_t command);
static void input_sector(struct channel *, void *);
static void output_sector(struct channel *, const void *);

static void wait_until_idle(const struct disk *);
static bool wait_while_busy(const struct disk *);
static void select_device(const struct disk *);
static void select_device_wait(const struct disk *);

static void interrupt_handler(struct intr_frame *);

/* Initialize the disk subsystem and detect disks. */
/* [한국어 번역] 디스크 하위 시스템을 초기화하고 디스크를 탐지한다. */
void disk_init(void)
{
    size_t chan_no;

    for (chan_no = 0; chan_no < CHANNEL_CNT; chan_no++) {
        struct channel *c = &channels[chan_no];
        int dev_no;

        /* Initialize channel. */
        /* [한국어 번역] 채널을 초기화한다. */
        snprintf(c->name, sizeof c->name, "hd%zu", chan_no);
        switch (chan_no) {
        case 0:
            c->reg_base = 0x1f0;
            c->irq = 14 + 0x20;
            break;
        case 1:
            c->reg_base = 0x170;
            c->irq = 15 + 0x20;
            break;
        default:
            NOT_REACHED();
        }
        lock_init(&c->lock);
        c->expecting_interrupt = false;
        sema_init(&c->completion_wait, 0);

        /* Initialize devices. */
        /* [한국어 번역] 장치들을 초기화한다. */
        for (dev_no = 0; dev_no < 2; dev_no++) {
            struct disk *d = &c->devices[dev_no];
            snprintf(d->name, sizeof d->name, "%s:%d", c->name, dev_no);
            d->channel = c;
            d->dev_no = dev_no;

            d->is_ata = false;
            d->capacity = 0;

            d->read_cnt = d->write_cnt = 0;
        }

        /* Register interrupt handler. */
        /* [한국어 번역] 인터럽트 핸들러를 등록한다. */
        intr_register_ext(c->irq, interrupt_handler, c->name);

        /* Reset hardware. */
        /* [한국어 번역] 하드웨어를 리셋한다. */
        reset_channel(c);

        /* Distinguish ATA hard disks from other devices. */
        /* [한국어 번역] ATA 하드 디스크와 다른 장치들을 구분한다. */
        if (check_device_type(&c->devices[0])) {
            check_device_type(&c->devices[1]);
        }

        /* Read hard disk identity information. */
        /* [한국어 번역] 하드 디스크의 식별 정보를 읽는다. */
        for (dev_no = 0; dev_no < 2; dev_no++) {
            if (c->devices[dev_no].is_ata) {
                identify_ata_device(&c->devices[dev_no]);
            }
        }
    }

    /* DO NOT MODIFY BELOW LINES. */
    /* [한국어 번역] 아래 줄들을 수정하지 말 것. */
    register_disk_inspect_intr();
}

/* Prints disk statistics. */
/* [한국어 번역] 디스크 통계를 출력한다. */
void disk_print_stats(void)
{
    int chan_no;

    for (chan_no = 0; chan_no < CHANNEL_CNT; chan_no++) {
        int dev_no;

        for (dev_no = 0; dev_no < 2; dev_no++) {
            struct disk *d = disk_get(chan_no, dev_no);
            if (d != NULL && d->is_ata) {
                printf("%s: %lld reads, %lld writes\n", d->name, d->read_cnt,
                       d->write_cnt);
            }
        }
    }
}

/* Returns the disk numbered DEV_NO--either 0 or 1 for master or
   slave, respectively--within the channel numbered CHAN_NO.

   Pintos uses disks this way:
0:0 - boot loader, command line args, and operating system kernel
0:1 - file system
1:0 - scratch
1:1 - swap
*/
/* [한국어 번역]
 * CHAN_NO번 채널에서 DEV_NO번 디스크를 반환한다.
 * DEV_NO는 마스터일 때 0, 슬레이브일 때 1이다.
 *
 * Pintos는 디스크를 다음과 같이 사용한다.
 * 0:0 - 부트 로더, 명령줄 인자, 운영체제 커널
 * 0:1 - 파일 시스템
 * 1:0 - 임시 작업용
 * 1:1 - 스왑
 */
struct disk *disk_get(int chan_no, int dev_no)
{
    ASSERT(dev_no == 0 || dev_no == 1);

    if (chan_no < (int)CHANNEL_CNT) {
        struct disk *d = &channels[chan_no].devices[dev_no];
        if (d->is_ata) {
            return d;
        }
    }
    return NULL;
}

/* Returns the size of disk D, measured in DISK_SECTOR_SIZE-byte
   sectors. */
/* [한국어 번역] 디스크 D의 크기를 DISK_SECTOR_SIZE바이트 크기의 섹터 수로 반환한다. */
disk_sector_t disk_size(struct disk *d)
{
    ASSERT(d != NULL);

    return d->capacity;
}

/* Reads sector SEC_NO from disk D into BUFFER, which must have
   room for DISK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to disks, so external
   per-disk locking is unneeded. */
/* [한국어 번역]
 * 디스크 D의 SEC_NO번 섹터를 BUFFER로 읽는다.
 * BUFFER에는 DISK_SECTOR_SIZE바이트를 저장할 공간이 있어야 한다.
 * 디스크 접근을 내부에서 동기화하므로 외부에서 디스크별 락을 걸 필요가 없다.
 */
void disk_read(struct disk *d, disk_sector_t sec_no, void *buffer)
{
    struct channel *c;

    ASSERT(d != NULL);
    ASSERT(buffer != NULL);

    c = d->channel;
    lock_acquire(&c->lock);
    select_sector(d, sec_no);
    issue_pio_command(c, CMD_READ_SECTOR_RETRY);
    sema_down(&c->completion_wait);
    if (!wait_while_busy(d)) {
        PANIC("%s: disk read failed, sector=%" PRDSNu, d->name, sec_no);
    }
    input_sector(c, buffer);
    d->read_cnt++;
    lock_release(&c->lock);
}

/* Write sector SEC_NO to disk D from BUFFER, which must contain
   DISK_SECTOR_SIZE bytes.  Returns after the disk has
   acknowledged receiving the data.
   Internally synchronizes accesses to disks, so external
   per-disk locking is unneeded. */
/* [한국어 번역]
 * BUFFER의 내용을 디스크 D의 SEC_NO번 섹터에 기록한다.
 * BUFFER에는 DISK_SECTOR_SIZE바이트의 데이터가 있어야 한다.
 * 디스크가 데이터 수신을 확인한 뒤 반환한다.
 * 디스크 접근을 내부에서 동기화하므로 외부에서 디스크별 락을 걸 필요가 없다.
 */
void disk_write(struct disk *d, disk_sector_t sec_no, const void *buffer)
{
    struct channel *c;

    ASSERT(d != NULL);
    ASSERT(buffer != NULL);

    c = d->channel;
    lock_acquire(&c->lock);
    select_sector(d, sec_no);
    issue_pio_command(c, CMD_WRITE_SECTOR_RETRY);
    if (!wait_while_busy(d)) {
        PANIC("%s: disk write failed, sector=%" PRDSNu, d->name, sec_no);
    }
    output_sector(c, buffer);
    sema_down(&c->completion_wait);
    d->write_cnt++;
    lock_release(&c->lock);
}

/* Disk detection and identification. */
/* [한국어 번역] 디스크 탐지와 식별. */

static void print_ata_string(char *string, size_t size);

/* Resets an ATA channel and waits for any devices present on it
   to finish the reset. */
/* [한국어 번역] ATA 채널을 리셋하고, 해당 채널에 있는 장치들의 리셋이 끝날 때까지 기다린다. */
static void reset_channel(struct channel *c)
{
    bool present[2];
    int dev_no;

    /* The ATA reset sequence depends on which devices are present,
	   so we start by detecting device presence. */
    /* [한국어 번역]
     * ATA 리셋 순서는 어떤 장치가 있는지에 따라 달라지므로
     * 먼저 장치가 있는지 탐지한다.
     */
    for (dev_no = 0; dev_no < 2; dev_no++) {
        struct disk *d = &c->devices[dev_no];

        select_device(d);

        outb(reg_nsect(c), 0x55);
        outb(reg_lbal(c), 0xaa);

        outb(reg_nsect(c), 0xaa);
        outb(reg_lbal(c), 0x55);

        outb(reg_nsect(c), 0x55);
        outb(reg_lbal(c), 0xaa);

        present[dev_no] =
            (inb(reg_nsect(c)) == 0x55 && inb(reg_lbal(c)) == 0xaa);
    }

    /* Issue soft reset sequence, which selects device 0 as a side effect.
	   Also enable interrupts. */
    /* [한국어 번역]
     * 소프트 리셋 순서를 수행한다. 이 과정에서 장치 0이 선택된다.
     * 인터럽트도 활성화한다.
     */
    outb(reg_ctl(c), 0);
    timer_usleep(10);
    outb(reg_ctl(c), CTL_SRST);
    timer_usleep(10);
    outb(reg_ctl(c), 0);

    timer_msleep(150);

    /* Wait for device 0 to clear BSY. */
    /* [한국어 번역] 장치 0의 BSY 비트가 해제될 때까지 기다린다. */
    if (present[0]) {
        select_device(&c->devices[0]);
        wait_while_busy(&c->devices[0]);
    }

    /* Wait for device 1 to clear BSY. */
    /* [한국어 번역] 장치 1의 BSY 비트가 해제될 때까지 기다린다. */
    if (present[1]) {
        int i;

        select_device(&c->devices[1]);
        for (i = 0; i < 3000; i++) {
            if (inb(reg_nsect(c)) == 1 && inb(reg_lbal(c)) == 1) {
                break;
            }
            timer_msleep(10);
        }
        wait_while_busy(&c->devices[1]);
    }
}

/* Checks whether device D is an ATA disk and sets D's is_ata
   member appropriately.  If D is device 0 (master), returns true
   if it's possible that a slave (device 1) exists on this
   channel.  If D is device 1 (slave), the return value is not
   meaningful. */
/* [한국어 번역]
 * 장치 D가 ATA 디스크인지 검사하고 D의 is_ata 멤버를 알맞게 설정한다.
 * D가 장치 0(마스터)이면 이 채널에 장치 1(슬레이브)이 있을 가능성이 있을 때
 * true를 반환한다. D가 장치 1(슬레이브)이면 반환값은 의미가 없다.
 */
static bool check_device_type(struct disk *d)
{
    struct channel *c = d->channel;
    uint8_t error, lbam, lbah, status;

    select_device(d);

    error = inb(reg_error(c));
    lbam = inb(reg_lbam(c));
    lbah = inb(reg_lbah(c));
    status = inb(reg_status(c));

    if ((error != 1 && (error != 0x81 || d->dev_no == 1)) ||
        (status & STA_DRDY) == 0 || (status & STA_BSY) != 0) {
        d->is_ata = false;
        return error != 0x81;
    } else {
        d->is_ata = (lbam == 0 && lbah == 0) || (lbam == 0x3c && lbah == 0xc3);
        return true;
    }
}

/* Sends an IDENTIFY DEVICE command to disk D and reads the
   response.  Initializes D's capacity member based on the result
   and prints a message describing the disk to the console. */
/* [한국어 번역]
 * 디스크 D에 IDENTIFY DEVICE 명령을 보내고 응답을 읽는다.
 * 그 결과로 D의 capacity 멤버를 초기화하고,
 * 디스크를 설명하는 메시지를 콘솔에 출력한다.
 */
static void identify_ata_device(struct disk *d)
{
    struct channel *c = d->channel;
    uint16_t id[DISK_SECTOR_SIZE / 2];

    ASSERT(d->is_ata);

    /* Send the IDENTIFY DEVICE command, wait for an interrupt
	   indicating the device's response is ready, and read the data
	   into our buffer. */
    /* [한국어 번역]
     * IDENTIFY DEVICE 명령을 보내고, 장치 응답이 준비되었음을 알리는
     * 인터럽트를 기다린 다음, 데이터를 버퍼로 읽는다.
     */
    select_device_wait(d);
    issue_pio_command(c, CMD_IDENTIFY_DEVICE);
    sema_down(&c->completion_wait);
    if (!wait_while_busy(d)) {
        d->is_ata = false;
        return;
    }
    input_sector(c, id);

    /* Calculate capacity. */
    /* [한국어 번역] 용량을 계산한다. */
    d->capacity = id[60] | ((uint32_t)id[61] << 16);

    /* Print identification message. */
    /* [한국어 번역] 식별 메시지를 출력한다. */
    printf("%s: detected %'" PRDSNu " sector (", d->name, d->capacity);
    if (d->capacity > 1024 / DISK_SECTOR_SIZE * 1024 * 1024) {
        printf("%" PRDSNu " GB",
               d->capacity / (1024 / DISK_SECTOR_SIZE * 1024 * 1024));
    } else if (d->capacity > 1024 / DISK_SECTOR_SIZE * 1024) {
        printf("%" PRDSNu " MB",
               d->capacity / (1024 / DISK_SECTOR_SIZE * 1024));
    } else if (d->capacity > 1024 / DISK_SECTOR_SIZE) {
        printf("%" PRDSNu " kB", d->capacity / (1024 / DISK_SECTOR_SIZE));
    } else {
        printf("%" PRDSNu " byte", d->capacity * DISK_SECTOR_SIZE);
    }
    printf(") disk, model \"");
    print_ata_string((char *)&id[27], 40);
    printf("\", serial \"");
    print_ata_string((char *)&id[10], 20);
    printf("\"\n");
}

/* Prints STRING, which consists of SIZE bytes in a funky format:
   each pair of bytes is in reverse order.  Does not print
   trailing whitespace and/or nulls. */
/* [한국어 번역]
 * SIZE바이트로 이루어진 STRING을 출력한다.
 * 이 문자열은 각 바이트 쌍의 순서가 뒤집힌 특수한 형식이다.
 * 끝부분의 공백 문자나 널 문자는 출력하지 않는다.
 */
static void print_ata_string(char *string, size_t size)
{
    size_t i;

    /* Find the last non-white, non-null character. */
    /* [한국어 번역] 끝에서부터 공백 문자도 널 문자도 아닌 마지막 문자를 찾는다. */
    for (; size > 0; size--) {
        int c = string[(size - 1) ^ 1];
        if (c != '\0' && !isspace(c)) {
            break;
        }
    }

    /* Print. */
    /* [한국어 번역] 출력한다. */
    for (i = 0; i < size; i++) {
        printf("%c", string[i ^ 1]);
    }
}

/* Selects device D, waiting for it to become ready, and then
   writes SEC_NO to the disk's sector selection registers.  (We
   use LBA mode.) */
/* [한국어 번역]
 * 장치 D를 선택하고 준비될 때까지 기다린 다음,
 * 디스크의 섹터 선택 레지스터에 SEC_NO를 기록한다. LBA 모드를 사용한다.
 */
static void select_sector(struct disk *d, disk_sector_t sec_no)
{
    struct channel *c = d->channel;

    ASSERT(sec_no < d->capacity);
    ASSERT(sec_no < (1UL << 28));

    select_device_wait(d);
    outb(reg_nsect(c), 1);
    outb(reg_lbal(c), sec_no);
    outb(reg_lbam(c), sec_no >> 8);
    outb(reg_lbah(c), (sec_no >> 16));
    outb(reg_device(c),
         DEV_MBS | DEV_LBA | (d->dev_no == 1 ? DEV_DEV : 0) | (sec_no >> 24));
}

/* Writes COMMAND to channel C and prepares for receiving a
   completion interrupt. */
/* [한국어 번역] 채널 C에 COMMAND를 기록하고 완료 인터럽트를 받을 준비를 한다. */
static void issue_pio_command(struct channel *c, uint8_t command)
{
    /* Interrupts must be enabled or our semaphore will never be
	   up'd by the completion handler. */
    /* [한국어 번역]
     * 인터럽트가 활성화되어 있어야 한다.
     * 그렇지 않으면 완료 핸들러가 세마포어를 올릴 수 없다.
     */
    ASSERT(intr_get_level() == INTR_ON);

    c->expecting_interrupt = true;
    outb(reg_command(c), command);
}

/* Reads a sector from channel C's data register in PIO mode into
   SECTOR, which must have room for DISK_SECTOR_SIZE bytes. */
/* [한국어 번역]
 * PIO 모드로 채널 C의 데이터 레지스터에서 섹터 하나를 SECTOR로 읽는다.
 * SECTOR에는 DISK_SECTOR_SIZE바이트를 저장할 공간이 있어야 한다.
 */
static void input_sector(struct channel *c, void *sector)
{
    insw(reg_data(c), sector, DISK_SECTOR_SIZE / 2);
}

/* Writes SECTOR to channel C's data register in PIO mode.
   SECTOR must contain DISK_SECTOR_SIZE bytes. */
/* [한국어 번역]
 * PIO 모드로 SECTOR의 내용을 채널 C의 데이터 레지스터에 기록한다.
 * SECTOR에는 DISK_SECTOR_SIZE바이트의 데이터가 있어야 한다.
 */
static void output_sector(struct channel *c, const void *sector)
{
    outsw(reg_data(c), sector, DISK_SECTOR_SIZE / 2);
}

/* Low-level ATA primitives. */
/* [한국어 번역] 저수준 ATA 기본 연산. */

/* Wait up to 10 seconds for the controller to become idle, that
   is, for the BSY and DRQ bits to clear in the status register.

   As a side effect, reading the status register clears any
   pending interrupt. */
/* [한국어 번역]
 * 컨트롤러가 유휴 상태가 될 때까지 최대 10초 동안 기다린다.
 * 이는 상태 레지스터의 BSY와 DRQ 비트가 모두 해제된 상태를 뜻한다.
 * 상태 레지스터를 읽으면 대기 중인 인터럽트가 해제되는 부수 효과가 있다.
 */
static void wait_until_idle(const struct disk *d)
{
    int i;

    for (i = 0; i < 1000; i++) {
        if ((inb(reg_status(d->channel)) & (STA_BSY | STA_DRQ)) == 0) {
            return;
        }
        timer_usleep(10);
    }

    printf("%s: idle timeout\n", d->name);
}

/* Wait up to 30 seconds for disk D to clear BSY,
   and then return the status of the DRQ bit.
   The ATA standards say that a disk may take as long as that to
   complete its reset. */
/* [한국어 번역]
 * 디스크 D의 BSY 비트가 해제될 때까지 최대 30초 동안 기다린 다음,
 * DRQ 비트의 상태를 반환한다.
 * ATA 표준에 따르면 디스크의 리셋 완료에는 이만큼의 시간이 걸릴 수 있다.
 */
static bool wait_while_busy(const struct disk *d)
{
    struct channel *c = d->channel;
    int i;

    for (i = 0; i < 3000; i++) {
        if (i == 700) {
            printf("%s: busy, waiting...", d->name);
        }
        if (!(inb(reg_alt_status(c)) & STA_BSY)) {
            if (i >= 700) {
                printf("ok\n");
            }
            return (inb(reg_alt_status(c)) & STA_DRQ) != 0;
        }
        timer_msleep(10);
    }

    printf("failed\n");
    return false;
}

/* Program D's channel so that D is now the selected disk. */
/* [한국어 번역] D가 현재 선택된 디스크가 되도록 D의 채널을 설정한다. */
static void select_device(const struct disk *d)
{
    struct channel *c = d->channel;
    uint8_t dev = DEV_MBS;
    if (d->dev_no == 1) {
        dev |= DEV_DEV;
    }
    outb(reg_device(c), dev);
    inb(reg_alt_status(c));
    timer_nsleep(400);
}

/* Select disk D in its channel, as select_device(), but wait for
   the channel to become idle before and after. */
/* [한국어 번역]
 * select_device()처럼 D의 채널에서 디스크 D를 선택하되,
 * 선택 전후에 채널이 유휴 상태가 될 때까지 기다린다.
 */
static void select_device_wait(const struct disk *d)
{
    wait_until_idle(d);
    select_device(d);
    wait_until_idle(d);
}

/* ATA interrupt handler. */
/* [한국어 번역] ATA 인터럽트 핸들러. */
static void interrupt_handler(struct intr_frame *f)
{
    struct channel *c;

    for (c = channels; c < channels + CHANNEL_CNT; c++) {
        if (f->vec_no == c->irq) {
            if (c->expecting_interrupt) {
                inb(reg_status(c));           /* Acknowledge interrupt. */
                /* [한국어 번역] 인터럽트를 확인하고 처리했음을 알린다. */
                sema_up(&c->completion_wait); /* Wake up waiter. */
                /* [한국어 번역] 대기 중인 실행 주체를 깨운다. */
            } else {
                printf("%s: unexpected interrupt\n", c->name);
            }
            return;
        }
    }

    NOT_REACHED();
}

static void inspect_read_cnt(struct intr_frame *f)
{
    struct disk *d = disk_get(f->R.rdx, f->R.rcx);
    f->R.rax = d->read_cnt;
}

static void inspect_write_cnt(struct intr_frame *f)
{
    struct disk *d = disk_get(f->R.rdx, f->R.rcx);
    f->R.rax = d->write_cnt;
}

/* Tool for testing disk r/w cnt. Calling this function via int 0x43 and int 0x44.
 * Input:
 *   @RDX - chan_no of disk to inspect
 *   @RCX - dev_no of disk to inspect
 * Output:
 *   @RAX - Read/Write count of disk. */
/* [한국어 번역]
 * 디스크 읽기·쓰기 횟수를 검사하는 도구.
 * 인터럽트 0x43과 0x44를 통해 이 함수를 호출한다.
 * 입력:
 *   @RDX - 검사할 디스크의 채널 번호(chan_no)
 *   @RCX - 검사할 디스크의 장치 번호(dev_no)
 * 출력:
 *   @RAX - 디스크의 읽기·쓰기 횟수.
 */
void register_disk_inspect_intr(void)
{
    intr_register_int(0x43, 3, INTR_OFF, inspect_read_cnt,
                      "Inspect Disk Read Count");
    intr_register_int(0x44, 3, INTR_OFF, inspect_write_cnt,
                      "Inspect Disk Write Count");
}
