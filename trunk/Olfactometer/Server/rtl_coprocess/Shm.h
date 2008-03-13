#ifndef OlfCoprocessShm_H
#define OlfCoprocessShm_H

#define OlfCoprocessShm_MAGIC ((int)0xf3231337)
#define OlfCoprocessShm_NAME "OlfCoprocessShm"

struct OlfCoprocessShm
{
  int magic;
  unsigned cmd_fifo, datalog_fifo;
};

#endif
