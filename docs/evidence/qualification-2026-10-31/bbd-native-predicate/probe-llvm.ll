; ModuleID = '/Users/runner/work/yamaha-cs80/yamaha-cs80/driver/tests/diagnostics/BbdHistoryPredicateProbe.cpp'
source_filename = "/Users/runner/work/yamaha-cs80/yamaha-cs80/driver/tests/diagnostics/BbdHistoryPredicateProbe.cpp"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx15.0.0"

@.str = private unnamed_addr constant [59 x i8] c"mxcsr=%08x item=%u expected=%u production=%u candidate=%u\0A\00", align 1
@.str.1 = private unnamed_addr constant [71 x i8] c"Original production predicate failures=%u; original MXCSR restored=%u\0A\00", align 1

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind ssp willreturn memory(none) uwtable
define noundef zeroext i1 @production_probe(float noundef %0) local_unnamed_addr #0 {
  %2 = fcmp une float %0, 0.000000e+00
  ret i1 %2
}

; Function Attrs: mustprogress nofree noinline norecurse nounwind ssp memory(inaccessiblemem: readwrite) uwtable
define zeroext i1 @integer_observation_probe(float noundef %0) local_unnamed_addr #1 {
  %2 = alloca i32, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %2)
  %3 = bitcast float %0 to i32
  store volatile i32 %3, ptr %2, align 4, !tbaa !6
  %4 = load volatile i32, ptr %2, align 4, !tbaa !6
  %5 = and i32 %4, 2147483647
  %6 = icmp ne i32 %5, 0
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %2)
  ret i1 %6
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress norecurse nounwind ssp uwtable
define range(i32 0, 2) i32 @main() local_unnamed_addr #3 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca float, align 4
  %4 = alloca float, align 4
  %5 = alloca float, align 4
  %6 = alloca float, align 4
  %7 = alloca i32, align 4
  %8 = alloca i32, align 4
  %9 = alloca i32, align 4
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %1)
  %10 = load i32, ptr %1, align 4
  %11 = and i32 %10, -32833
  store i32 %11, ptr %2, align 4
  call void @llvm.x86.sse.ldmxcsr(ptr nonnull %2)
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %3)
  store volatile float 0x36A0000000000000, ptr %3, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %4)
  store volatile float 0xB6A0000000000000, ptr %4, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %5)
  store volatile float 0.000000e+00, ptr %5, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %6)
  store volatile float -0.000000e+00, ptr %6, align 4, !tbaa !10
  %12 = load volatile float, ptr %3, align 4, !tbaa !10
  %13 = call zeroext i1 @production_probe(float noundef %12) #5
  %14 = load volatile float, ptr %4, align 4, !tbaa !10
  %15 = call zeroext i1 @production_probe(float noundef %14) #5
  %16 = load volatile float, ptr %5, align 4, !tbaa !10
  %17 = call zeroext i1 @production_probe(float noundef %16) #5
  %18 = load volatile float, ptr %6, align 4, !tbaa !10
  %19 = call zeroext i1 @production_probe(float noundef %18) #5
  %20 = load volatile float, ptr %3, align 4, !tbaa !10
  %21 = call zeroext i1 @integer_observation_probe(float noundef %20) #5
  %22 = load volatile float, ptr %4, align 4, !tbaa !10
  %23 = call zeroext i1 @integer_observation_probe(float noundef %22) #5
  %24 = load volatile float, ptr %5, align 4, !tbaa !10
  %25 = call zeroext i1 @integer_observation_probe(float noundef %24) #5
  %26 = load volatile float, ptr %6, align 4, !tbaa !10
  %27 = call zeroext i1 @integer_observation_probe(float noundef %26) #5
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %28 = load i32, ptr %7, align 4
  %29 = zext i1 %13 to i32
  %30 = zext i1 %21 to i32
  %31 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %28, i32 noundef 0, i32 noundef 1, i32 noundef %29, i32 noundef %30)
  %32 = xor i1 %13, true
  %33 = zext i1 %32 to i32
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %34 = load i32, ptr %7, align 4
  %35 = zext i1 %15 to i32
  %36 = zext i1 %23 to i32
  %37 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %34, i32 noundef 1, i32 noundef 1, i32 noundef %35, i32 noundef %36)
  %38 = xor i1 %15, true
  %39 = zext i1 %38 to i32
  %40 = add nuw nsw i32 %33, %39
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %41 = load i32, ptr %7, align 4
  %42 = zext i1 %17 to i32
  %43 = zext i1 %25 to i32
  %44 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %41, i32 noundef 2, i32 noundef 0, i32 noundef %42, i32 noundef %43)
  %45 = add nuw nsw i32 %40, %42
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %46 = load i32, ptr %7, align 4
  %47 = zext i1 %19 to i32
  %48 = zext i1 %27 to i32
  %49 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %46, i32 noundef 3, i32 noundef 0, i32 noundef %47, i32 noundef %48)
  %50 = add nuw nsw i32 %45, %47
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %6)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %5)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %4)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %3)
  %51 = or disjoint i32 %11, 64
  store i32 %51, ptr %2, align 4
  call void @llvm.x86.sse.ldmxcsr(ptr nonnull %2)
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %3)
  store volatile float 0x36A0000000000000, ptr %3, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %4)
  store volatile float 0xB6A0000000000000, ptr %4, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %5)
  store volatile float 0.000000e+00, ptr %5, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %6)
  store volatile float -0.000000e+00, ptr %6, align 4, !tbaa !10
  %52 = load volatile float, ptr %3, align 4, !tbaa !10
  %53 = call zeroext i1 @production_probe(float noundef %52) #5
  %54 = load volatile float, ptr %4, align 4, !tbaa !10
  %55 = call zeroext i1 @production_probe(float noundef %54) #5
  %56 = load volatile float, ptr %5, align 4, !tbaa !10
  %57 = call zeroext i1 @production_probe(float noundef %56) #5
  %58 = load volatile float, ptr %6, align 4, !tbaa !10
  %59 = call zeroext i1 @production_probe(float noundef %58) #5
  %60 = load volatile float, ptr %3, align 4, !tbaa !10
  %61 = call zeroext i1 @integer_observation_probe(float noundef %60) #5
  %62 = load volatile float, ptr %4, align 4, !tbaa !10
  %63 = call zeroext i1 @integer_observation_probe(float noundef %62) #5
  %64 = load volatile float, ptr %5, align 4, !tbaa !10
  %65 = call zeroext i1 @integer_observation_probe(float noundef %64) #5
  %66 = load volatile float, ptr %6, align 4, !tbaa !10
  %67 = call zeroext i1 @integer_observation_probe(float noundef %66) #5
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %68 = load i32, ptr %7, align 4
  %69 = zext i1 %53 to i32
  %70 = zext i1 %61 to i32
  %71 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %68, i32 noundef 0, i32 noundef 1, i32 noundef %69, i32 noundef %70)
  %72 = xor i1 %53, true
  %73 = zext i1 %72 to i32
  %74 = add nuw nsw i32 %50, %73
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %75 = load i32, ptr %7, align 4
  %76 = zext i1 %55 to i32
  %77 = zext i1 %63 to i32
  %78 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %75, i32 noundef 1, i32 noundef 1, i32 noundef %76, i32 noundef %77)
  %79 = xor i1 %55, true
  %80 = zext i1 %79 to i32
  %81 = add nuw nsw i32 %74, %80
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %82 = load i32, ptr %7, align 4
  %83 = zext i1 %57 to i32
  %84 = zext i1 %65 to i32
  %85 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %82, i32 noundef 2, i32 noundef 0, i32 noundef %83, i32 noundef %84)
  %86 = add nuw nsw i32 %81, %83
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %87 = load i32, ptr %7, align 4
  %88 = zext i1 %59 to i32
  %89 = zext i1 %67 to i32
  %90 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %87, i32 noundef 3, i32 noundef 0, i32 noundef %88, i32 noundef %89)
  %91 = add i32 %86, %88
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %6)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %5)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %4)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %3)
  %92 = or disjoint i32 %11, 32768
  store i32 %92, ptr %2, align 4
  call void @llvm.x86.sse.ldmxcsr(ptr nonnull %2)
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %3)
  store volatile float 0x36A0000000000000, ptr %3, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %4)
  store volatile float 0xB6A0000000000000, ptr %4, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %5)
  store volatile float 0.000000e+00, ptr %5, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %6)
  store volatile float -0.000000e+00, ptr %6, align 4, !tbaa !10
  %93 = load volatile float, ptr %3, align 4, !tbaa !10
  %94 = call zeroext i1 @production_probe(float noundef %93) #5
  %95 = load volatile float, ptr %4, align 4, !tbaa !10
  %96 = call zeroext i1 @production_probe(float noundef %95) #5
  %97 = load volatile float, ptr %5, align 4, !tbaa !10
  %98 = call zeroext i1 @production_probe(float noundef %97) #5
  %99 = load volatile float, ptr %6, align 4, !tbaa !10
  %100 = call zeroext i1 @production_probe(float noundef %99) #5
  %101 = load volatile float, ptr %3, align 4, !tbaa !10
  %102 = call zeroext i1 @integer_observation_probe(float noundef %101) #5
  %103 = load volatile float, ptr %4, align 4, !tbaa !10
  %104 = call zeroext i1 @integer_observation_probe(float noundef %103) #5
  %105 = load volatile float, ptr %5, align 4, !tbaa !10
  %106 = call zeroext i1 @integer_observation_probe(float noundef %105) #5
  %107 = load volatile float, ptr %6, align 4, !tbaa !10
  %108 = call zeroext i1 @integer_observation_probe(float noundef %107) #5
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %109 = load i32, ptr %7, align 4
  %110 = zext i1 %94 to i32
  %111 = zext i1 %102 to i32
  %112 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %109, i32 noundef 0, i32 noundef 1, i32 noundef %110, i32 noundef %111)
  %113 = xor i1 %94, true
  %114 = zext i1 %113 to i32
  %115 = add i32 %91, %114
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %116 = load i32, ptr %7, align 4
  %117 = zext i1 %96 to i32
  %118 = zext i1 %104 to i32
  %119 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %116, i32 noundef 1, i32 noundef 1, i32 noundef %117, i32 noundef %118)
  %120 = xor i1 %96, true
  %121 = zext i1 %120 to i32
  %122 = add i32 %115, %121
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %123 = load i32, ptr %7, align 4
  %124 = zext i1 %98 to i32
  %125 = zext i1 %106 to i32
  %126 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %123, i32 noundef 2, i32 noundef 0, i32 noundef %124, i32 noundef %125)
  %127 = add i32 %122, %124
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %128 = load i32, ptr %7, align 4
  %129 = zext i1 %100 to i32
  %130 = zext i1 %108 to i32
  %131 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %128, i32 noundef 3, i32 noundef 0, i32 noundef %129, i32 noundef %130)
  %132 = add i32 %127, %129
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %6)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %5)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %4)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %3)
  %133 = or i32 %10, 32832
  store i32 %133, ptr %2, align 4
  call void @llvm.x86.sse.ldmxcsr(ptr nonnull %2)
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %3)
  store volatile float 0x36A0000000000000, ptr %3, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %4)
  store volatile float 0xB6A0000000000000, ptr %4, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %5)
  store volatile float 0.000000e+00, ptr %5, align 4, !tbaa !10
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %6)
  store volatile float -0.000000e+00, ptr %6, align 4, !tbaa !10
  %134 = load volatile float, ptr %3, align 4, !tbaa !10
  %135 = call zeroext i1 @production_probe(float noundef %134) #5
  %136 = load volatile float, ptr %4, align 4, !tbaa !10
  %137 = call zeroext i1 @production_probe(float noundef %136) #5
  %138 = load volatile float, ptr %5, align 4, !tbaa !10
  %139 = call zeroext i1 @production_probe(float noundef %138) #5
  %140 = load volatile float, ptr %6, align 4, !tbaa !10
  %141 = call zeroext i1 @production_probe(float noundef %140) #5
  %142 = load volatile float, ptr %3, align 4, !tbaa !10
  %143 = call zeroext i1 @integer_observation_probe(float noundef %142) #5
  %144 = load volatile float, ptr %4, align 4, !tbaa !10
  %145 = call zeroext i1 @integer_observation_probe(float noundef %144) #5
  %146 = load volatile float, ptr %5, align 4, !tbaa !10
  %147 = call zeroext i1 @integer_observation_probe(float noundef %146) #5
  %148 = load volatile float, ptr %6, align 4, !tbaa !10
  %149 = call zeroext i1 @integer_observation_probe(float noundef %148) #5
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %150 = load i32, ptr %7, align 4
  %151 = zext i1 %135 to i32
  %152 = zext i1 %143 to i32
  %153 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %150, i32 noundef 0, i32 noundef 1, i32 noundef %151, i32 noundef %152)
  %154 = xor i1 %135, true
  %155 = zext i1 %154 to i32
  %156 = add i32 %132, %155
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %157 = load i32, ptr %7, align 4
  %158 = zext i1 %137 to i32
  %159 = zext i1 %145 to i32
  %160 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %157, i32 noundef 1, i32 noundef 1, i32 noundef %158, i32 noundef %159)
  %161 = xor i1 %137, true
  %162 = zext i1 %161 to i32
  %163 = add i32 %156, %162
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %164 = load i32, ptr %7, align 4
  %165 = zext i1 %139 to i32
  %166 = zext i1 %147 to i32
  %167 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %164, i32 noundef 2, i32 noundef 0, i32 noundef %165, i32 noundef %166)
  %168 = add i32 %163, %165
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %7)
  %169 = load i32, ptr %7, align 4
  %170 = zext i1 %141 to i32
  %171 = zext i1 %149 to i32
  %172 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %169, i32 noundef 3, i32 noundef 0, i32 noundef %170, i32 noundef %171)
  %173 = add i32 %168, %170
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %6)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %5)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %4)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %3)
  store i32 %10, ptr %8, align 4
  call void @llvm.x86.sse.ldmxcsr(ptr nonnull %8)
  call void @llvm.x86.sse.stmxcsr(ptr nonnull %9)
  %174 = load i32, ptr %9, align 4
  %175 = icmp eq i32 %174, %10
  %176 = zext i1 %175 to i32
  %177 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %173, i32 noundef %176)
  %178 = icmp ne i32 %173, 0
  %179 = zext i1 %178 to i32
  ret i32 %179
}

; Function Attrs: nounwind memory(argmem: write)
declare void @llvm.x86.sse.stmxcsr(ptr) #4

; Function Attrs: nounwind
declare void @llvm.x86.sse.ldmxcsr(ptr) #5

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #6

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind ssp willreturn memory(none) uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cmov,+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nofree noinline norecurse nounwind ssp memory(inaccessiblemem: readwrite) uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cmov,+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #2 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { mustprogress norecurse nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cmov,+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind memory(argmem: write) }
attributes #5 = { nounwind }
attributes #6 = { nofree nounwind "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cmov,+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 15, i32 5]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 17.0.0 (clang-1700.0.13.5)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = !{!11, !11, i64 0}
!11 = !{!"float", !8, i64 0}
