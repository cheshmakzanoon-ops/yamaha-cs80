                for (int k = 0; k < CP_count; ++k)
                    std::swap(p.values[chParam(0, k)], p.values[chParam(1, k)]);
                for (int k = 0; k < 4; ++k)
                    std::swap(p.values[bodyParam(0, k)], p.values[bodyParam(1, k)]);
                std::swap(p.values[bodySizeParam(0)], p.values[bodySizeParam(1)]);
                std::swap(p.values[bodyVariantParam(0)], p.values[bodyVariantParam(1)]);
                std::swap(p.values[bodyMaterialParam(0)], p.values[bodyMaterialParam(1)]);
                p.values[P_mix] = 1.0f - p.values[P_mix];
                // The named card stays on concert pitch after the rank swap.
                // DETUNE II is one-sided, so a negative compensation would
                // be outside the hardware control range.
                p.values[P_detune] = 0.0f;
