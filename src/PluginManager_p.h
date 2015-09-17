#pragma once

#include <boost/optional.hpp>
#include <string>
#include <unordered_map>
#include <tuple>
#include <sstream>
#include <msgpack.hpp>
#include <msgpack/rpc/protocol.h>
#include <QObject>
#include <QProcess>
#include <QLocalSocket>
#include <QLocalServer>
#include <QEventLoop>
#include <QTimer>

#include "core/macros.h"
#include "core/msgpackHelper.h"
#include "core/stlSpecialization.h"
#include "core/Singleton.h"
#include "core/IContext.h"
#include "core/IKeyEventFilter.h"
#include "CommandArgument.h"

class QKeyEvent;
class PluginManager;

class ResponseResult : public QObject {
  Q_OBJECT
  DISABLE_COPY_AND_MOVE(ResponseResult)

 public:
  ResponseResult() = default;
  ~ResponseResult() { qDebug("~ResponseResult"); }

  bool isReady() { return m_isReady; }
  bool isSuccess() { return m_isSuccess; }
  msgpack::object result() { return m_result->object; }

  void setResult(std::unique_ptr<object_with_zone> obj);
  void setError(std::unique_ptr<object_with_zone> obj);

signals:
  void ready();

 private:
  bool m_isReady;
  bool m_isSuccess;
  std::unique_ptr<object_with_zone> m_result;
};

class PluginManagerPrivate : public QObject, public core::IKeyEventFilter {
  Q_OBJECT
 public:
  static std::unordered_map<QString, std::function<void(const QString&, const msgpack::object&)>>
      s_notifyFunctions;
  static std::unordered_map<
      QString,
      std::function<void(msgpack::rpc::msgid_t, const QString&, const msgpack::object&)>>
      s_requestFunctions;
  static msgpack::rpc::msgid_t s_msgId;
  static std::unordered_map<msgpack::rpc::msgid_t, ResponseResult*> s_eventLoopMap;

  PluginManager* q;
  std::unique_ptr<QProcess> m_pluginProcess;
  QLocalSocket* m_socket;
  QLocalServer* m_server;
  bool m_isStopped;

  static constexpr auto TIMEOUT_IN_MS = 1000;

  explicit PluginManagerPrivate(PluginManager* q_ptr);
  ~PluginManagerPrivate();

  void init();

  void sendFocusChangedEvent(const QString& viewType);
  void sendCommandEvent(const QString& command, const CommandArgument& args);
  void callExternalCommand(const QString& cmd, const CommandArgument& args);
  bool askExternalContext(const QString& name, core::Operator op, const QString& value);
  QString translate(const std::string& key, const QString& defaultValue);

  // IKeyEventFilter interface
  bool keyEventFilter(QKeyEvent* event) override;
  void readStdout();
  void readStderr();
  void pluginRunnerConnected();
  void onFinished(int exitCode);
  void readRequest();
  void displayError(QLocalSocket::LocalSocketError);
  std::tuple<bool, std::string, CommandArgument> cmdEventFilter(const std::string& name,
                                                                const CommandArgument& arg);
  void callRequestFunc(const QString& method,
                       msgpack::rpc::msgid_t msgId,
                       const msgpack::object& obj);
  void callNotifyFunc(const QString& method, const msgpack::object& obj);

  template <typename Parameter>
  void sendNotification(const std::string& method, const Parameter& params) {
    if (m_isStopped) {
      return;
    }

    if (!m_socket) {
      qWarning("socket has not been initialized yet");
      return;
    }

    msgpack::sbuffer sbuf;
    msgpack::rpc::msg_notify<std::string, Parameter> notify;
    notify.method = method;
    notify.param = params;
    msgpack::pack(sbuf, notify);

    m_socket->write(sbuf.data(), sbuf.size());
  }

  /**
   * @brief Send a request via msgpack rpc.
   * Throws an exception if the response type is not an expected type.
   * @param method
   * @param params
   * @param type
   * @return return a response as the expected type.
   */
  template <typename Parameter, typename Result>
  Result sendRequest(const std::string& method,
                     const Parameter& params,
                     msgpack::type::object_type type) {
    std::unique_ptr<ResponseResult> result = sendRequestInternal<Parameter, Result>(method, params);
    if (result->result().type == type) {
      return result->result().as<Result>();
    } else {
      throw std::runtime_error("unexpected result type");
    }
  }

  /**
   * @brief Send a request via msgpack rpc.
   * Returns none if the response type is msgpack::Nil
   * Throws an exception if the response type is not null and an expected type.
   * @param method
   * @param params
   * @param type
   * @return return a response as the specified type.
   */
  template <typename Parameter, typename Result>
  boost::optional<Result> sendRequestOption(const std::string& method,
                                            const Parameter& params,
                                            msgpack::type::object_type type) {
    std::unique_ptr<ResponseResult> result = sendRequestInternal<Parameter, Result>(method, params);
    if (result->result().type == msgpack::type::NIL) {
      return boost::none;
    } else if (result->result().type == type) {
      return result->result().as<Result>();
    } else {
      throw std::runtime_error("unexpected result type");
    }
  }

  template <typename Result, typename Error>
  void sendResponse(const Result& res, const Error& err, msgpack::rpc::msgid_t id) {
    if (m_isStopped) {
      return;
    }

    msgpack::sbuffer sbuf;
    msgpack::rpc::msg_response<Result, Error> response;
    response.msgid = id;
    response.error = err;
    response.result = res;

    msgpack::pack(sbuf, response);

    m_socket->write(sbuf.data(), sbuf.size());
  }

  void sendError(const std::string& err, msgpack::rpc::msgid_t id);

  void startPluginRunnerProcess();

 private:
  // Send a request via msgpack rpc.
  template <typename Parameter, typename Result>
  std::unique_ptr<ResponseResult> sendRequestInternal(const std::string& method,
                                                      const Parameter& params) {
    if (m_isStopped) {
      throw std::runtime_error("plugin runner is not running");
    }

    qDebug("sendRequest. method: %s", method.c_str());
    msgpack::sbuffer sbuf;
    msgpack::rpc::msg_request<std::string, Parameter> request;
    request.method = method;
    msgpack::rpc::msgid_t msgId = s_msgId++;
    request.msgid = msgId;
    request.param = params;
    msgpack::pack(sbuf, request);

    if (!m_socket) {
      throw std::runtime_error("socket has not been initialized yet");
    }
    m_socket->write(sbuf.data(), sbuf.size());

    QEventLoop loop;
    std::unique_ptr<ResponseResult> result(new ResponseResult());
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    s_eventLoopMap.insert(std::make_pair(msgId, result.get()));
    connect(result.get(), &ResponseResult::ready, &loop, &QEventLoop::quit);

    timer.start(TIMEOUT_IN_MS);
    // start a local event loop to wait until plugin runner returns response or timeout occurs
    loop.exec();

    if (result->isReady()) {
      if (result->isSuccess()) {
        return std::move(result);
      } else {
        std::string errMsg = result->result().as<std::string>();
        throw std::runtime_error(errMsg);
      }
    } else {
      throw std::runtime_error("timeout for waiting the result");
    }
  }
};