/* Copyright 2022 ETH Zurich and University of Bologna. */
/* Licensed under the Apache License, Version 2.0, see LICENSE for details. */
/* SPDX-License-Identifier: Apache-2.0 */

/* Nicole Narr <narrn@student.ethz.ch> */
/* Christopher Reinwardt <creinwar@student.ethz.ch> */
/* Paul Scheffler <paulsc@iis.ee.ethz.ch> */
/* Lorenzo Leone <lleone@iis.ee.ethz.ch> */


INCLUDE common.ldh

SECTIONS {
  .text : {
    *(.text._start)
    *(.text)
    *(.text.*)
  } > memisl

  .misc : ALIGN(16) {
    *(.rodata)
    *(.rodata.*)
    *(.data)
    *(.data.*)
    *(.srodata)
    *(.srodata.*)
    *(.sdata)
    *(.sdata.*)
  } > memisl

  . = ALIGN(32);
  __bss_start = .;
  .bss : {
    *(.bss)
    *(.bss.*)
    *(.sbss)
    *(.sbss.*)
  } > memisl
  . = ALIGN(32);
  __bss_end = .;

  /* Thread Local Storage sections */
  __tdata_start = .;
  .tdata    :
  {
    *(.tdata .tdata.* .gnu.linkonce.td.*)
  } > memisl
  __tdata_end = .;

  . = ALIGN(32);
  __tbss_start = .;
  .tbss :
  {
    *(.tbss .tbss.* .gnu.linkonce.tb.*)
    *(.tcommon)
  } > memisl
  . = ALIGN(32);
  __tbss_end = .;

  /* Cluster Local Storage sections */
  __cdata_start = .;
  .cdata    :
  {
    *(.cdata .cdata.*)
  } > memisl
  __cdata_end = .;

  . = ALIGN(32);
  __cbss_start = .;
  .cbss :
  {
    *(.cbss .cbss.*)
    /* KEEP(*(.cbss .cbss.*)) */
  } > memisl
  . = ALIGN(32);
  __cbss_end = .;


  .heap : ALIGN(16) {
    __heap_start = .;
    __heap_end = ORIGIN(memisl) + LENGTH(memisl);
  } > memisl

  .bulk : ALIGN(16) {
    *(.bulk)
    *(.bulk.*)
  } > memisl

  .dram :
  {
    *(.dram)
    _edram = .;
  } > dram
}
