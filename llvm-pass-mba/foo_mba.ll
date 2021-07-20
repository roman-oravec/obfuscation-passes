; ModuleID = 'foo.ll'
source_filename = "foo.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 5, i32* %2, align 4
  store i32 6, i32* %3, align 4
  %4 = load i32, i32* %2, align 4
  %5 = load i32, i32* %3, align 4
  %6 = icmp sgt i32 %4, %5
  br i1 %6, label %7, label %12

7:                                                ; preds = %0
  %8 = load i32, i32* %2, align 4
  %9 = sub nsw i32 %8, 3
  store i32 %9, i32* %2, align 4
  %10 = load i32, i32* %3, align 4
  %11 = add nsw i32 %10, 3
  store i32 %11, i32* %3, align 4
  store i32 7, i32* %1, align 4
  br label %17

12:                                               ; preds = %0
  %13 = load i32, i32* %2, align 4
  %14 = add nsw i32 %13, 3
  store i32 %14, i32* %2, align 4
  %15 = load i32, i32* %3, align 4
  %16 = sub nsw i32 %15, 3
  store i32 %16, i32* %3, align 4
  store i32 8, i32* %1, align 4
  br label %17

17:                                               ; preds = %12, %7
  %18 = load i32, i32* %1, align 4
  ret i32 %18
}

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.1-16 "}
