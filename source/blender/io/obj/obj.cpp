#include "obj.h"

bool export_obj(bContext *C, OBJExportParams * a){
  if (a->print_name) {
    printf("\n Ankit");
  }
  if (a->number) {
    printf("\n%f\n",a->number);
  }
  return true;
}
