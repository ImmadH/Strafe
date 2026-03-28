#include "app.h"
#include <iostream>

int main()
{
  if (!App::Init())
    return -1;

  App::mainLoop();
  return 0;
}
