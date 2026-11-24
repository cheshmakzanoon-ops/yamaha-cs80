#pragma once

// Small, authored spatial pickup proxies for the electric-piano bodies.
// Pfeifle, DAFx-17, sections 4, 5.4 and 5.6 distinguishes the nearly sinusoidal
// mechanical motion from the electrical output: magnetic voltage follows
// changing flux, while the biased reed pickup follows changing capacitance.
// Moving the resting position changes that conversion, without retuning the
// vibrating element. https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_79.pdf
//
// These dimensionless proxies are not the paper's measured/FEM field model:
//   magnetic flux: Phi(x) = 1 / (1 + x*x), x = offset + a*cos(theta)
//   capacitance:   C(x)   = 1 / (gap - x), x = a*cos(theta).
// We differentiate their Fourier series analytically and retain a finite
// harmonic bank. No audio sample is passed through either nonlinear function.
// The caller applies its host-Nyquist taper to every retained harmonic.
//
// The fundamental coefficient is one: the external rank VCA owns overall
// playing amplitude. Harmonic ratios follow excursion, including its decay;
// a separate, velocity-independent full-strike bound supplies headroom. This
// does not normalize a softer strike by its smaller total spectral energy.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace cs80::body
{

class ElectricPickup
{
public:
    static constexpr int harmonicCount = 16;
    using Spectrum = std::array<double, harmonicCount>;

    void configure(bool capacitive, double size, double velocity) noexcept
    {
        reed = capacitive;
        size = finiteClamp(size, 0.5, 2.5, 1.0);
        velocity = finiteClamp(velocity, 0.0, 1.0, 0.7);
        // Size changes the pickup's resting geometry, never oscillator pitch.
        // Magnetic alignment moves along a smooth field flank. The reed's
        // effective plate clearance grows with size. Brightness below changes
        // the excursion/gap ratio, as a closer pickup increases upper tone.
        offset = 0.665 + 0.11 * (size - 1.0);
        gap = 1.0 + 0.10 * (size - 1.0);
        strikeExcursion = 0.22 + 0.78 * velocity;
    }

    Spectrum coefficients(double brightness, double motion) const noexcept
    {
        const double colour = finiteClamp(brightness, 0.0, 1.0, 0.5);
        const double excursion = finiteClamp(motion, 0.0, 1.0, 0.0);
        const double amplitude = (reed ? 0.38 + 0.35 * colour : 0.42 + 0.46 * colour)
                               * strikeExcursion * excursion;
        Spectrum result {};
        result[0] = 1.0;
        if (amplitude == 0.0) return result;

        if (reed)
        {
            // The normalized derivative of 1/(g-a*cos(theta)) has harmonic
            // ratios n*q^(n-1), q=a/(g+sqrt(g*g-a*a)). The minimum clearance
            // is 0.22 at every admitted size/touch/brightness combination.
            const double root = std::sqrt(gap * gap - amplitude * amplitude);
            const double q = amplitude / (gap + root);
            double power = q;
            for (int harmonic = 2; harmonic <= harmonicCount; ++harmonic)
            {
                result[static_cast<std::size_t>(harmonic - 1)] = harmonic * power;
                power *= q;
            }
        }
        else
        {
            const Magnetic root = magnetic(amplitude);
            double real = root.real, imaginary = root.imaginary;
            for (int harmonic = 2; harmonic <= harmonicCount; ++harmonic)
            {
                // Signs retain the phase relations of the differentiated
                // field. An even partial can smoothly cross zero as the
                // excursion recedes across an asymmetric resting geometry.
                result[static_cast<std::size_t>(harmonic - 1)]
                    = harmonic * (real + root.quadrature * imaginary);
                const double next = real * root.real - imaginary * root.imaginary;
                imaginary = imaginary * root.real + real * root.imaginary;
                real = next;
            }
        }
        return result;
    }

    double fullStrikeBound() const noexcept
    {
        // For the magnetic series a_n = n*Re((1-i*k)*w^(n-1)). Both |w|
        // and k increase with excursion on 0 < offset < 1, so this bound at
        // maximum brightness/velocity covers every colour, strike and tail.
        // The capacitor coefficients are positive and monotone in excursion.
        const Magnetic reference = magnetic(0.88);
        const double maximum = reed
            ? 0.73 / (gap + std::sqrt(gap * gap - 0.73 * 0.73))
            : std::hypot(reference.real, reference.imaginary);
        const double projection = reed ? 1.0 : std::hypot(1.0, reference.quadrature);
        double bound = 1.0, power = maximum;
        for (int harmonic = 2; harmonic <= harmonicCount; ++harmonic)
        {
            bound += projection * harmonic * power;
            power *= maximum;
        }
        return bound;
    }

private:
    struct Magnetic { double real, imaginary, quadrature; };
    bool reed = false;
    double offset = 0.665, gap = 1.0, strikeExcursion = 0.766;

    static double finiteClamp(double value, double low, double high, double fallback) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    }

    Magnetic magnetic(double amplitude) const noexcept
    {
        // Put z=offset+i and s=sqrt(z*z-a*a), choosing Im(s)>0. Expanding
        // 1/(z+a*cos(theta)) gives w=-a/(z+s). Since Phi=-Im(1/(x+i)), its
        // differentiated nth coefficient is proportional to n*Im(w^n/s).
        // Factoring out a before dividing by the first coefficient avoids
        // loss of precision as the excursion approaches zero.
        const double difference = offset * offset - 1.0 - amplitude * amplitude;
        const double radial = std::sqrt(difference * difference + 4.0 * offset * offset);
        const double rootImaginary = std::sqrt(0.5 * (radial - difference));
        const double rootReal = offset / rootImaginary;
        const double sumReal = offset + rootReal;
        const double sumImaginary = 1.0 + rootImaginary;
        const double denominator = sumReal * sumReal + sumImaginary * sumImaginary;
        // k=Re(-1/((z+s)*s))/Im(-1/((z+s)*s)). With q=Im(s)>=1,
        // k=(q^3-offset^2)/(offset*q*(q+1)) is positive and increasing.
        const double quadrature = (rootImaginary * rootImaginary * rootImaginary - offset * offset)
            / (offset * rootImaginary * (rootImaginary + 1.0));
        return {-amplitude * sumReal / denominator,
                 amplitude * sumImaginary / denominator, quadrature};
    }
};

} // namespace cs80::body
