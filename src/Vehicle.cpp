//
// Created by Marcin Gierlicki on 6/2/18.
//

#include <math.h>
#include "vehicle.h"

Vehicle::Vehicle(int id, double x, double y, double vx, double vy, double s, double d)
    : id(id), x(x), y(y), vx(vx), vy(vy), s(s), d(d) {
}

double Vehicle::speed() {
  return sqrt(vx * vx + vy * vy);
}

double Vehicle::IsInLane(int lane_index) {
  return d > lane_index * 4.0 && d < (lane_index + 1) * 4.0;
}

double Vehicle::FutureS(double delta_t) {
  return s + delta_t * speed();
}
