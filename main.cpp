#include "homewindow.h"  // 自定义主窗口类
#include <QApplication>  // Qt 应用程序核心类
#include "easylogging++.h"  // 第三方日志库

INITIALIZE_EASYLOGGINGPP    // 初始化宏，有且只能使用一次

#undef main  //取消 main 的宏定义（某些环境如 SDL 会定义 main 为宏）
int main(int argc, char *argv[])
{

//    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Format, "%datetime %level %func(L%line) %msg");

/*
  ┌──────────────────┬────────────────────────────┐
  │      配置项      │            说明            │
  ├──────────────────┼────────────────────────────┤
  │ Format           │ 日志格式：`[时间           │
  ├──────────────────┼────────────────────────────┤
  │ Filename         │ 日志文件名：log_年月日.log │
  ├──────────────────┼────────────────────────────┤
  │ ToFile           │ 保存到文件                 │
  ├──────────────────┼────────────────────────────┤
  │ ToStandardOutput │ 同时输出到终端             │
  └──────────────────┴────────────────────────────┘
*/

    el::Configurations conf;
    conf.setToDefault();
    conf.setGlobally(el::ConfigurationType::Format, "[%datetime | %level] %func(L%line) %msg");
    conf.setGlobally(el::ConfigurationType::Filename, "log_%datetime{%Y%M%d}.log");
    conf.setGlobally(el::ConfigurationType::Enabled, "true");
    conf.setGlobally(el::ConfigurationType::ToFile, "true");
    el::Loggers::reconfigureAllLoggers(conf);
    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToStandardOutput, "true"); // 也输出一份到终端

//    LOG(VERBOSE) << "logger test"; //该级别只能用宏VLOG而不能用宏 LOG(VERBOSE)
    LOG(TRACE) << " logger";
//    LOG(DEBUG) << "logger test";
    LOG(INFO) << "logger test";
    LOG(WARNING) << "logger test";
    LOG(ERROR) << "logger test";
    QApplication a(argc, argv);  // 创建 Qt 应用程序对象
    HomeWindow w;                 // 创建主窗口对象
    w.show();                     // 显示窗口
    return a.exec();              // 进入事件循环，等待用户交互
}
