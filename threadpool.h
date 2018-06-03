#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace RGR
{
#define MAX_THREAD_NUM 256
	// 线程池：管理一个线程队列，一个任务队列，然后每次取任务给线程去做，循环往复
	// 功能：可以提交变参函数或lambda表达式的匿名函数，可以获取执行返回值
	// 不支持类成员函数，支持类静态成员函数或全局函数，Operator()函数等

	class threadpool
	{
	private:
		using Task = std::function<void()>;
		
		// 线程池
		std::vector<std::thread>	m_pool;
		
		// 任务队列
		std::queue<Task>			m_tasks;
		std::mutex					m_tasks_mutex;
		std::condition_variable		m_tasks_cv;

		// 是否关闭提交
		std::atomic<bool>			m_stoped;
		// 空闲线程数量
		std::atomic<int>			m_idle_thnum;

	public:
		threadpool(size_t size = 3) :m_stoped(false)
		{
			m_idle_thnum = size < 1 ? 1 : size;
			for (int i = 0; i < m_idle_thnum; ++i)
			{
				m_pool.emplace_back([this] {
					//线程执行函数
					while (!this->m_stoped)
					{
						Task task;
						{
							std::unique_lock<std::mutex> lk(this->m_tasks_mutex);
							this->m_tasks_cv.wait(lk, [this] {
								return this->m_stoped.load() || !this->m_tasks.empty();
							});// wait 直到有task 或者 关闭提交任务

							if (this->m_stoped && this->m_tasks.empty())
							{
								return;
							}

							task = std::move(this->m_tasks.front());
							this->m_tasks.pop();
						}
						m_idle_thnum--;
						task();
						m_idle_thnum++;
					}
				});
			}/* for end*/
		}

		~threadpool()
		{
			m_stoped.store(true);
			m_tasks_cv.notify_all();
			for (std::thread & thread : m_pool)
			{
				// thread.detach(); // 让线程自生自灭
				if (thread.joinable())
				{
					thread.join();  // 等待任务结束， 前提：线程一定会执行完
				}
			}
		}


		// 提交任务
		// 返回值类型为future, 调用.get()会等待任务完成，获取返回值
		// 有两种方法可以实现调用类成员
		// 1、bind: .commit(std::bind(&Dog::sayHello, &dog));
		// 2、mem_fn: .commit(std::mem_fun(&Dog::sayHello, &dog));
		template<class F, class... Args>
		auto commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
		{
			if (m_stoped.load())
			{
				throw std::runtime_error("commit on threadpool is stopped. ");
			}
			using RetType = decltype(f(args...));
			auto task = std::make_shared<std::packaged_task<RetType()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
				);

			std::future<RetType> future = task->get_future();
			{
				// 添加任务到队列
				std::lock_guard<std::mutex> lk(m_tasks_mutex);
				m_tasks.emplace( [task] { (*task)(); } );
			}

			m_tasks_cv.notify_one();  // 唤醒一个线程去执行

			return future;
		}

		int idleCount() { return m_idle_thnum; }
	};
}