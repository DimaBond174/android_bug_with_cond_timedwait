# android_bug_with_cond_timedwait
Found a bug in Android pthread_cond_timedwait

I discovered that the android sleeps on the pthread_cond_timedwait longer than expected.
The research showed that this happens because the monotonous clock stops during Doze sleep mode.
And since Android uses a monotonous clock for sleep mode, the problem is observed for all types of waiting: CLOCK_REALTIME and CLOCK_MONOTONIC.

How to use this Persuasion Stand:
1. Install Demo app on real hardware Android device
2. Turn off device display
3. Execute

```bash
adb shell dumpsys battery unplug
adb shell am set-inactive com.example.testcondtimewait true
adb shell dumpsys deviceidle enable

adb shell dumpsys deviceidle force-idle
adb shell dumpsys deviceidle step
```

4. Unplug device from computer
5. Wait for 30min
6. Plug device to computer with Android Studio
7. Download log from app Files folder

Example in this repo: 2023-09-28_16-34-30.txt
You can see:
```txt
[2023-09-28 16:39:30][w][523471715504]:WorkCondVar: before pthread_cond_timedwait

[2023-09-28 16:42:55][w][523472755888]:Work5sec
// Android slept here
[2023-09-28 16:57:49][w][523472755888]:Work5sec
...
[2023-09-28 17:00:18][w][523472755888]:Work5sec
[2023-09-28 17:00:22][w][523471715504]:WorkCondVar: after pthread_cond_timedwait
```
WorkCondVar slept longer then 5 minutes.
[This is due to](https://github.com/aosp-mirror/platform_bionic/blob/main/libc/bionic/bionic_futex.cpp#L46C1-L47C101) 

```c++
  // We have seen numerous bugs directly attributable to this difference.  Therefore, we provide
  // this general workaround to always use CLOCK_MONOTONIC for waiting, regardless of what the input
  // timespec is.
  timespec converted_monotonic_abs_timeout;
  if (abs_timeout && use_realtime_clock) {
    monotonic_time_from_realtime_time(converted_monotonic_abs_timeout, *abs_timeout);
    if (converted_monotonic_abs_timeout.tv_sec < 0) {
      return -ETIMEDOUT;
    }
    futex_abs_timeout = &converted_monotonic_abs_timeout;
  }

  return __futex(ftx, op, value, futex_abs_timeout, bitset);
}

```


