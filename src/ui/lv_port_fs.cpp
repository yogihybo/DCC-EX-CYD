#include "lv_port_fs.h"
#include <lvgl.h>
#include <FS.h>
#include <Settings.h>

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    fs::FS& fs = Settings.getFS();
    const char * flags = "";
    if(mode == LV_FS_MODE_WR) flags = FILE_WRITE;
    else if(mode == LV_FS_MODE_RD) flags = FILE_READ;
    else if(mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) flags = FILE_WRITE;
    
    String fullPath = "/";
    fullPath += path;
    
    File* f = new File(fs.open(fullPath, flags));
    if(!*f) {
        delete f;
        return NULL;
    }
    return f;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
    File* f = (File*)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    File* f = (File*)file_p;
    *br = f->read((uint8_t*)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw) {
    File* f = (File*)file_p;
    *bw = f->write((const uint8_t*)buf, btw);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    File* f = (File*)file_p;
    SeekMode mode = SeekSet;
    if(whence == LV_FS_SEEK_SET) mode = SeekSet;
    else if(whence == LV_FS_SEEK_CUR) mode = SeekCur;
    else if(whence == LV_FS_SEEK_END) mode = SeekEnd;
    f->seek(pos, mode);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    File* f = (File*)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

void lv_port_fs_init(void) {
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = 'S';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    lv_fs_drv_register(&fs_drv);
}
