// Will be used to implement the control of the robot, including movement and sensor integration but in C++.
// Entire code below AI generated and should be reviewed for correctness and safety before use in a real-world application.
// Here to provide a basic structure for a Furuta Pendulum LQR controller in C++17.
#include <iostream>
#include <cmath>

class FurutaLQRController {
private:
    // State-feedback gain matrix K = [k1, k2, k3, k4]
    // Corresponding to [theta1 (arm), theta2 (pendulum), dot_theta1, dot_theta2]
    double K[4];
    
    // Saturation limit for motor input (e.g., voltage or PWM duty cycle)
    double max_output;

public:
    FurutaLQRController(double k1, double k2, double k3, double k4, double max_out) 
        : max_output(max_out) {
        K[0] = k1;
        K[1] = k2;
        K[2] = k3;
        K[3] = k4;
    }

    /**
     * Compute control input u = -K * x
     * @param theta1    Rotary arm angle (radians, 0 = center)
     * @param theta2    Pendulum pole angle (radians, 0 = upright unstable equilibrium)
     * @param d_theta1  Angular velocity of arm
     * @param d_theta2  Angular velocity of pendulum
     * @return Clamped control signal (torque/voltage)
     */
    double computeControl(double theta1, double theta2, double d_theta1, double d_theta2) {
        // State vector x: [theta1, theta2, d_theta1, d_theta2]
        double u = -(K[0] * theta1 + K[1] * theta2 + K[2] * d_theta1 + K[3] * d_theta2);

        // Apply saturation / voltage limit
        if (u > max_output) u = max_output;
        if (u < -max_output) u = -max_output;

        return u;
    }

    void updateGains(double k1, double k2, double k3, double k4) {
        K[0] = k1;
        K[1] = k2;
        K[2] = k3;
        K[3] = k4;
    }
};

// Example Usage Loop
int main() {
    // Example LQR gains (tune these to your specific physical parameters)
    // State order: [arm_pos, pendulum_pos, arm_vel, pendulum_vel]
    FurutaLQRController controller(-12.5, -45.8,-4.2, -6.1, 12.0);

    // Simulated sensor readings near upright balance point
    double theta1 = 0.02;   // slight arm offset
    double theta2 = 0.015;  // slight pole offset from upright (0 rad)
    double d_theta1 = 0.05;
    double d_theta2 = -0.1;

    double control_signal = controller.computeControl(theta1, theta2, d_theta1, d_theta2);

    std::cout << "Computed Motor Control Input (u): " << control_signal << " V" << std::endl;
    return 0;
}
