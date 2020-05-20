#include "obj.h"

bool obj_export(bContext *C, OBJExportParams * a){
  if (a->print_name) {
    printf("\n OP");
  }
  if (a->number) {
    printf("\n%f\n",a->number);
  }
  return true;
}
