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
	// �̳߳أ�����һ���̶߳��У�һ��������У�Ȼ��ÿ��ȡ������߳�ȥ����ѭ������
	// ���ܣ������ύ��κ�����lambda���ʽ���������������Ի�ȡִ�з���ֵ
	// ��֧�����Ա������֧���ྲ̬��Ա������ȫ�ֺ�����Operator()������

	class threadpool
	{
	private:
		using Task = std::function<void()>;
		
		// �̳߳�
		std::vector<std::thread>	m_pool;
		
		// �������
		std::queue<Task>			m_tasks;
		std::mutex					m_tasks_mutex;
		std::condition_variable		m_tasks_cv;

		// �Ƿ�ر��ύ
		std::atomic<bool>			m_stoped;
		// �����߳�����
		std::atomic<int>			m_idle_thnum;

	public:
		threadpool(size_t size = 3) :m_stoped(false)
		{
			m_idle_thnum = size < 1 ? 1 : size;
			for (int i = 0; i < m_idle_thnum; ++i)
			{
				m_pool.emplace_back([this] {
					//�߳�ִ�к���
					while (!this->m_stoped)
					{
						Task task;
						{
							std::unique_lock<std::mutex> lk(this->m_tasks_mutex);
							this->m_tasks_cv.wait(lk, [this] {
								return this->m_stoped.load() || !this->m_tasks.empty();
							});// wait ֱ����task ���� �ر��ύ����

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
				// thread.detach(); // ���߳���������
				if (thread.joinable())
				{
					thread.join();  // �ȴ���������� ǰ�᣺�߳�һ����ִ����
				}
			}
		}


		// �ύ����
		// ����ֵ����Ϊfuture, ����.get()��ȴ�������ɣ���ȡ����ֵ
		// �����ַ�������ʵ�ֵ������Ա
		// 1��bind: .commit(std::bind(&Dog::sayHello, &dog));
		// 2��mem_fn: .commit(std::mem_fun(&Dog::sayHello, &dog));
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
				// ������񵽶���
				std::lock_guard<std::mutex> lk(m_tasks_mutex);
				m_tasks.emplace( [task] { (*task)(); } );
			}

			m_tasks_cv.notify_one();  // ����һ���߳�ȥִ��

			return future;
		}

		int idleCount() { return m_idle_thnum; }
	};
}