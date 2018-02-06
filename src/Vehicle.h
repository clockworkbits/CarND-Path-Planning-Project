//
// Created by Marcin Gierlicki on 6/2/18.
//

#ifndef PATH_PLANNING_VEHICLE_H
#define PATH_PLANNING_VEHICLE_H


class Vehicle {
public:
  const int id;
  const double x;
  const double y;
  const double vx;
  const double vy;
  const double s;
  const double d;

  Vehicle(int id, double x, double y, double vx, double vy, double s, double d);
  double speed();
  double IsInLane(int lane_index);
  double FutureS(double delta_t);
};


#endif //PATH_PLANNING_VEHICLE_H
