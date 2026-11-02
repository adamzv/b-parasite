#include <stdio.h>

#include "bparasite_build_identity.h"
#include "zboss_api_addons.h"

zb_zcl_status_t zb_zcl_set_attr_val(uint8_t endpoint, uint16_t cluster,
                                    uint8_t role, uint16_t attribute,
                                    uint8_t* data, uint8_t check_access) {
  return 0;
}

int main(void) {
  struct zb_device_ctx ctx = {0};
  prst_zb_attrs_init(&ctx);
  if (ctx.basic_attr.sw_ver[0] != 15 ||
      memcmp(ctx.basic_attr.sw_ver + 1, PRST_FIRMWARE_BUILD_ID, 15) != 0) {
    fputs("FAIL: Basic swBuildId does not contain generated identity\n", stderr);
    return 1;
  }
  if (ctx.basic_attr.date_code[0] != 8 ||
      memcmp(ctx.basic_attr.date_code + 1, "20261028", 8) != 0) {
    fputs("FAIL: Basic dateCode does not contain generated date\n", stderr);
    return 1;
  }
  puts("PASS: Basic attributes expose generated identity and date");
  return 0;
}
