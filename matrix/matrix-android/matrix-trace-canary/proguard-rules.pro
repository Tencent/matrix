
-keep class com.tencent.matrix.trace.core.AppMethodBeat { *; }
-keep class com.tencent.matrix.trace.tracer.SignalAnrTracer {
    native <methods>;
}
-keep class com.tencent.matrix.trace.tracer.ThreadTracer {
    native <methods>;
}
-keep class com.tencent.matrix.trace.tracer.TouchEventLagTracer {
    native <methods>;
}