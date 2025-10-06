#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <ncurses.h>
#include <regex>
#include <cstdint>

struct ProcessInfo
{
    pid_t pid,ppid;
    char state;
    std::string cmd,cpus_allowed_list;
    long rss,num_threads,priority,nice;
    double mem_usage,cpu_usage,io_read_rate,io_write_rate,process_age,net_rx_rate,net_tx_rate;
    unsigned long utime,stime;
    uint64_t read_bytes,write_bytes,rchar,wchar,voluntary_ctxt_switches,shared_clean,private_dirty,fd_count,net_rx_bytes,net_tx_bytes;
};

struct Filter
{
    std::string key,op,value;
};

class ProcessAnalyzer
{
private:
    std::vector<ProcessInfo> processes,prev_processes;
    std::map<pid_t,std::vector<pid_t>> process_tree;
    std::map<pid_t,ProcessInfo> process_map;
    uint64_t mem_total=0,mem_free=0,prev_total_jiffies=0,prev_work_jiffies=0;
    double system_mem_usage=0.0,system_cpu_usage=0.0,poll_interval=1.0,system_uptime=0.0;
    int num_cores=0,selected_row=0,scroll_offset=0;
    long clk_tck=0;
    std::ofstream log_file;
    bool logging_enabled=false,tree_view=true,needs_redraw=true,show_only_zombies_orphans=false;
    std::string sort_criterion,status_msg;
    std::vector<Filter> filters;
    WINDOW *win;

    void scanProcesses()
    {
        processes.clear();
        process_tree.clear();
        process_map.clear();
        DIR *dir=opendir("/proc");
        if(!dir)
        {
            status_msg="Error: Cannot open /proc";
            return;
        }
        struct dirent *entry;
        std::string line;
        while(entry=readdir(dir))
        {
            if(entry->d_type!=DT_DIR||!isdigit(entry->d_name[0]))
            {
                continue;
            }
            pid_t pid=std::stoi(entry->d_name);
            ProcessInfo info;
            std::string path="/proc/"+std::to_string(pid)+"/stat";
            std::ifstream stat_file(path);
            if(!stat_file)
            {
                continue;
            }
            std::getline(stat_file,line);
            std::istringstream iss(line);
            int dummy;
            std::string cmd;
            uint64_t starttime;
            iss>>info.pid>>cmd>>info.state>>info.ppid>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>info.utime>>info.stime>>dummy>>dummy>>info.num_threads>>dummy>>dummy>>starttime>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>dummy>>info.priority>>info.nice;
            cmd=cmd.substr(1,cmd.size()-2);
            info.cmd=cmd;
            info.process_age=(system_uptime-static_cast<double>(starttime)/clk_tck)/3600.0;
            stat_file.close();
            path="/proc/"+std::to_string(pid)+"/status";
            std::ifstream status_file(path);
            if(status_file)
            {
                while(std::getline(status_file,line))
                {
                    std::istringstream iss(line);
                    std::string key;
                    iss>>key;
                    if(key=="VmRSS:")
                    {
                        iss>>info.rss;
                    }
                    else if(key=="voluntary_ctxt_switches:")
                    {
                        iss>>info.voluntary_ctxt_switches;
                    }
                    else if(key=="Cpus_allowed_list:")
                    {
                        iss>>info.cpus_allowed_list;
                    }
                }
                status_file.close();
            }
            path="/proc/"+std::to_string(pid)+"/io";
            std::ifstream io_file(path);
            if(io_file)
            {
                while(std::getline(io_file,line))
                {
                    std::istringstream iss(line);
                    std::string key;
                    iss>>key;
                    if(key=="read_bytes:")
                    {
                        iss>>info.read_bytes;
                    }
                    else if(key=="write_bytes:")
                    {
                        iss>>info.write_bytes;
                    }
                    else if(key=="rchar:")
                    {
                        iss>>info.rchar;
                    }
                    else if(key=="wchar:")
                    {
                        iss>>info.wchar;
                    }
                }
                io_file.close();
            }
            else
            {
                info.read_bytes=0;
                info.write_bytes=0;
                info.rchar=0;
                info.wchar=0;
            }
            path="/proc/"+std::to_string(pid)+"/smaps";
            std::ifstream smaps_file(path);
            if(smaps_file)
            {
                while(std::getline(smaps_file,line))
                {
                    std::istringstream iss(line);
                    std::string key;
                    uint64_t value;
                    iss>>key>>value;
                    if(key=="Shared_Clean:")
                    {
                        info.shared_clean+=value;
                    }
                    else if(key=="Private_Dirty:")
                    {
                        info.private_dirty+=value;
                    }
                }
                smaps_file.close();
            }
            else
            {
                info.shared_clean=0;
                info.private_dirty=0;
            }
            path="/proc/"+std::to_string(pid)+"/fd";
            DIR *fd_dir=opendir(path.c_str());
            info.fd_count=0;
            if(fd_dir)
            {
                while(entry=readdir(fd_dir))
                {
                    if(entry->d_type!=DT_DIR)
                    {
                        info.fd_count++;
                    }
                }
                closedir(fd_dir);
            }
            path="/proc/"+std::to_string(pid)+"/net/dev";
            std::ifstream net_file(path);
            if(net_file)
            {
                std::getline(net_file,line);
                std::getline(net_file,line);
                while(std::getline(net_file,line))
                {
                    std::istringstream iss(line);
                    std::string interface;
                    uint64_t rx_bytes,dummy,tx_bytes;
                    iss>>interface>>rx_bytes;
                    for(int i=0;i<7;i++)
                    {
                        iss>>dummy;
                    }
                    iss>>tx_bytes;
                    info.net_rx_bytes+=rx_bytes;
                    info.net_tx_bytes+=tx_bytes;
                }
                net_file.close();
            }
            else
            {
                info.net_rx_bytes=0;
                info.net_tx_bytes=0;
            }
            processes.push_back(info);
            process_map[pid]=info;
            process_tree[info.ppid].push_back(pid);
        }
        closedir(dir);
        std::ifstream mem_file("/proc/meminfo");
        while(std::getline(mem_file,line))
        {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            iss>>key>>value;
            if(key=="MemTotal:")
            {
                mem_total=value;
            }
            else if(key=="MemFree:")
            {
                mem_free=value;
            }
        }
        mem_file.close();
        system_mem_usage=100.0*(1.0-static_cast<double>(mem_free)/mem_total);
        for(auto &proc:processes)
        {
            proc.mem_usage=100.0*static_cast<double>(proc.rss*getpagesize())/(mem_total*1024);
        }
        num_cores=sysconf(_SC_NPROCESSORS_ONLN);
        uint64_t total_jiffies=0,work_jiffies=0;
        std::ifstream stat_file("/proc/stat");
        if(stat_file)
        {
            std::getline(stat_file,line);
            std::istringstream iss(line);
            std::string cpu;
            uint64_t user,nice,system,idle,iowait, irq,softirqirq,steal,guest,guest_nice;
            iss>>cpu>>user>>nice>>system>>idle>>iowait>> irq>>softirq irq>>steal>>guest>>guest_nice;
            work_jiffies=user+nice+system+irq+soft irq+steal+guest+guest_nice;
            total_jiffies=work_jiffies+idle+iowait;
            stat_file.close();
        }
        if(prev_total_jiffies>0)
        {
            system_cpu_usage=100.0*static_cast<double>(work_jiffies-prev_work_jiffies)/(total_jiffies-prev_total_jiffies);
        }
        else
        {
            system_cpu_usage=0.0;
        }
        for(auto &proc:processes)
        {
            auto it=std::find_if(prev_processes.begin(),prev_processes.end(),[&](const ProcessInfo &p)
            {
                return p.pid==proc.pid;
            });
            if(it!=prev_processes.end())
            {
                unsigned long delta_utime=proc.utime-it->utime;
                unsigned long delta_stime=proc.stime-it->stime;
                unsigned long delta_process=delta_utime+delta_stime;
                uint64_t delta_total=total_jiffies-prev_total_jiffies;
                if(delta_total>0)
                {
                    proc.cpu_usage=100.0*static_cast<double>(delta_process)/delta_total*num_cores;
                }
                else
                {
                    proc.cpu_usage=0.0;
                }
                uint64_t delta_read=proc.read_bytes-it->read_bytes;
                uint64_t delta_write=proc.write_bytes-it->write_bytes;
                proc.io_read_rate=static_cast<double>(delta_read)/1024.0/poll_interval;
                proc.io_write_rate=static_cast<double>(delta_write)/1024.0/poll_interval;
                uint64_t delta_rx=proc.net_rx_bytes-it->net_rx_bytes;
                uint64_t delta_tx=proc.net_tx_bytes-it->net_tx_bytes;
                proc.net_rx_rate=static_cast<double>(delta_rx)/1024.0/poll_interval;
                proc.net_tx_rate=static_cast<double>(delta_tx)/1024.0/poll_interval;
            }
            else
            {
                proc.cpu_usage=0.0;
                proc.io_read_rate=0.0;
                proc.io_write_rate=0.0;
                proc.net_rx_rate=0.0;
                proc.net_tx_rate=0.0;
            }
        }
        prev_processes=processes;
        prev_total_jiffies=total_jiffies;
        prev_work_jiffies=work_jiffies;
        needs_redraw=true;
    }

    std::vector<ProcessInfo> getZombiesAndOrphans()
    {
        std::vector<ProcessInfo> zombies_orphans;
        for(const auto &proc:processes)
        {
            if(proc.state=='Z'||(proc.ppid==1&&proc.pid!=1))
            {
                zombies_orphans.push_back(proc);
            }
        }
        return zombies_orphans;
    }

    bool promptKillZombiesOrphans()
    {
        auto zombies_orphans=getZombiesAndOrphans();
        if(zombies_orphans.empty())
        {
            return false;
        }

        werase(win);
        displayHeader();
        mvwprintw(win,2,0,"Found %d zombie/orphan processes. Kill them? (y/n): ",(int)zombies_orphans.size());
        wrefresh(win);

        char choice=getch();
        if(choice=='y'||choice=='Y')
        {
            int killed=0;
            for(const auto &proc:zombies_orphans)
            {
                pid_t kill_pid=(proc.state=='Z')?proc.ppid:proc.pid;
                if(kill(kill_pid,SIGTERM)==0)
                {
                    killed++;
                }
            }
            status_msg="Killed "+std::to_string(killed)+" processes";
            return true;
        }
        else
        {
            status_msg="Kill cancelled";
            return false;
        }
    }

    std::vector<Filter> parseFilters(std::string input)
    {
        std::vector<Filter> filters;
        if(input.empty())
        {
            return filters;
        }
        std::istringstream iss(input);
        std::string token;
        std::regex regex("(\\w+)([:<>])(.*)");
        while(iss>>token)
        {
            std::smatch match;
            if(std::regex_match(token,match,regex))
            {
                std::string key=match[1].str();
                std::string op=match[2].str();
                std::string value=match[3].str();
                if(key=="pid"||key=="ppid"||key=="state"||key=="cmd"||key=="cpu"||key=="mem"||key=="age")
                {
                    filters.push_back({key,op,value});
                }
                else
                {
                    status_msg="Invalid filter key: "+key;
                    return {};
                }
            }
            else
            {
                status_msg="Invalid filter format: "+token;
                return {};
            }
        }
        return filters;
    }

    bool matchesFilter(const ProcessInfo &proc,const Filter &filter)
    {
        if(filter.key=="pid")
        {
            try
            {
                pid_t val=std::stoi(filter.value);
                if(filter.op==":"&&proc.pid==val)
                {
                    return true;
                }
            }
            catch(...)
            {
                status_msg="Invalid PID: "+filter.value;
                return false;
            }
        }
        else if(filter.key=="ppid")
        {
            try
            {
                pid_t val=std::stoi(filter.value);
                if(filter.op==":"&&proc.ppid==val)
                {
                    return true;
                }
            }
            catch(...)
            {
                status_msg="Invalid PPID: "+filter.value;
                return false;
            }
        }
        else if(filter.key=="state")
        {
            if(filter.op==":"&&proc.state==filter.value[0])
            {
                return true;
            }
        }
        else if(filter.key=="cmd")
        {
            std::string cmd=proc.cmd;
            std::string val=filter.value;
            std::transform(cmd.begin(),cmd.end(),cmd.begin(),::tolower);
            std::transform(val.begin(),val.end(),val.begin(),::tolower);
            if(filter.op==":"&&cmd.find(val)!=std::string::npos)
            {
                return true;
            }
        }
        else if(filter.key=="cpu")
        {
            try
            {
                double val=std::stod(filter.value);
                if(filter.op==">"&&proc.cpu_usage>val)
                {
                    return true;
                }
                if(filter.op=="<"&&proc.cpu_usage<val)
                {
                    return true;
                }
                if(filter.op==":"&&std::abs(proc.cpu_usage-val)<0.01)
                {
                    return true;
                }
            }
            catch(...)
            {
                status_msg="Invalid CPU value: "+filter.value;
                return false;
            }
        }
        else if(filter.key=="mem")
        {
            try
            {
                double val=std::stod(filter.value);
                if(filter.op==">"&&proc.mem_usage>val)
                {
                    return true;
                }
                if(filter.op=="<"&&proc.mem_usage<val)
                {
                    return true;
                }
                if(filter.op==":"&&std::abs(proc.mem_usage-val)<0.01)
                {
                    return true;
                }
            }
            catch(...)
            {
                status_msg="Invalid Mem value: "+filter.value;
                return false;
            }
        }
        else if(filter.key=="age")
        {
            try
            {
                double val=std::stod(filter.value);
                if(filter.op==">"&&proc.process_age>val)
                {
                    return true;
                }
                if(filter.op=="<"&&proc.process_age<val)
                {
                    return true;
                }
                if(filter.op==":"&&std::abs(proc.process_age-val)<0.01)
                {
                    return true;
                }
            }
            catch(...)
            {
                status_msg="Invalid Age value: "+filter.value;
                return false;
            }
        }
        return false;
    }

    std::vector<ProcessInfo> filterProcesses()
    {
        std::vector<ProcessInfo> filtered;
        filtered.reserve(processes.size());
        for(const auto &proc:processes)
        {
            bool pass=true;
            for(const auto &filter:filters)
            {
                if(!matchesFilter(proc,filter))
                {
                    pass=false;
                    break;
                }
            }
            if(pass)
            {
                filtered.push_back(proc);
            }
        }
        return filtered;
    }

    void logProcesses()
    {
        if(!log_file.is_open())
        {
            log_file.open("process_log.csv",std::ios::app);
            if(!log_file)
            {
                status_msg="Error: Cannot open process_log.csv";
                return;
            }
            log_file<<"Timestamp,PID,PPID,State,Cmd,Mem%,CPU%,IO R (KB/s),IO W (KB/s),RChar (KB),WChar (KB),Shared (KB),Private (KB),FD,Threads,CtxtSw,Age (h),Priority,Nice,CPUs,Net R (KB/s),Net W (KB/s)\n";
        }
        if(log_file)
        {
            time_t now=time(nullptr);
            std::string ts=ctime(&now);
            ts.erase(std::remove(ts.begin(),ts.end(),'\n'),ts.end());
            for(const auto &proc:processes)
            {
                log_file<<ts<<","<<proc.pid<<","<<proc.ppid<<","<<proc.state<<","<<proc.cmd<<","<<proc.mem_usage<<","<<proc.cpu_usage<<","<<proc.io_read_rate<<","<<proc.io_write_rate<<","<<proc.rchar/1024<<","<<proc.wchar/1024<<","<<proc.shared_clean<<","<<proc.private_dirty<<","<<proc.fd_count<<","<<proc.num_threads<<","<<proc.voluntary_ctxt_switches<<","<<proc.process_age<<","<<proc.priority<<","<<proc.nice<<","<<proc.cpus_allowed_list<<","<<proc.net_rx_rate<<","<<proc.net_tx_rate<<"\n";
            }
            log_file.flush();
        }
    }

    void sortProcesses(std::string criterion)
    {
        if(criterion=="cpu")
        {
            std::sort(processes.begin(),processes.end(),[](const ProcessInfo &a,const ProcessInfo &b)
            {
                return a.cpu_usage>b.cpu_usage;
            });
        }
        else if(criterion=="mem")
        {
            std::sort(processes.begin(),processes.end(),[](const ProcessInfo &a,const ProcessInfo &b)
            {
                return a.mem_usage>b.mem_usage;
            });
        }
        else if(criterion=="io")
        {
            std::sort(processes.begin(),processes.end(),[](const ProcessInfo &a,const ProcessInfo &b)
            {
                return (a.io_read_rate+a.io_write_rate)>(b.io_read_rate+b.io_write_rate);
            });
        }
        else if(criterion=="net")
        {
            std::sort(processes.begin(),processes.end(),[](const ProcessInfo &a,const ProcessInfo &b)
            {
                return (a.net_rx_rate+a.net_tx_rate)>(b.net_rx_rate+b.net_tx_bytes);
            });
        }
        needs_redraw=true;
    }

    void displayHeader()
    {
        wattron(win,COLOR_PAIR(1));
        std::string filter_display=filters.empty()?"None":"";
        for(int i=0;i<filters.size();i++)
        {
            if(i>0)
            {
                filter_display+=" ";
            }
            filter_display+=filters[i].key+filters[i].op+filters[i].value;
        }
        mvwprintw(win,0,0,"System CPU: %.2f%% Mem: %.2f%% Uptime: %.2f h | Cores: %d | %s | Sort: %s | Filter: %s",
                  system_cpu_usage,system_mem_usage,system_uptime/3600.0,num_cores,
                  logging_enabled?"Logging: ON":"Logging: OFF",sort_criterion.empty()?"None":sort_criterion.c_str(),
                  filter_display.c_str());
        mvwprintw(win,1,0,"F4: Filter (e.g., pid:1234 cpu>50) | F5: Tree/List | F6: Sort | F9: Kill | Z: Zombie/Orphan | L: Log | Q: Quit");
        if(!status_msg.empty())
        {
            wattron(win,COLOR_PAIR(2));
            mvwprintw(win,2,0,"%s",status_msg.c_str());
            wattroff(win,COLOR_PAIR(2));
        }
        wattroff(win,COLOR_PAIR(1));
    }

    void displayTree(pid_t pid,int depth,int &line,int max_lines,int max_cols)
    {
        auto it=process_map.find(pid);
        if(it==process_map.end())
        {
            return;
        }

        // Skip if we're only showing zombies/orphans and this process isn't one
        if(show_only_zombies_orphans && !(it->second.state=='Z'||(it->second.ppid==1&&it->second.pid!=1)))
        {
            // Still need to traverse children for counting purposes
            auto children=process_tree.find(pid);
            if(children!=process_tree.end())
            {
                for(const auto &child:children->second)
                {
                    displayTree(child,depth+1,line,max_lines,max_cols);
                }
            }
            return;
        }

        if(line>=max_lines+scroll_offset||line<scroll_offset)
        {
            // Still need to increment line counter even if not displaying
            line++;
            auto children=process_tree.find(pid);
            if(children!=process_tree.end())
            {
                for(const auto &child:children->second)
                {
                    displayTree(child,depth+1,line,max_lines,max_cols);
                }
            }
            return;
        }
        if(line>=scroll_offset&&line<max_lines+scroll_offset)
        {
            std::string indent(depth*2,' ');
            std::string cmd=it->second.cmd;
            if(cmd.length()>20)
            {
                cmd=cmd.substr(0,17)+"...";
            }
            int color=(it->second.state=='Z')?3:(it->second.ppid==1&&it->second.pid!=1)?4:(it->second.cpu_usage>50.0)?2:1;
            if(line==selected_row)
            {
                wattron(win,A_REVERSE);
            }
            wattron(win,COLOR_PAIR(color));
            mvwprintw(win,line-scroll_offset+3,0,"%s%-5d %-5d %c %6.2f %6.2f %6.2f %6.2f %6llu %6llu %6llu %6llu %4llu %4ld %6llu %5.2f %3ld %3ld %-6s %6.2f %6.2f %-20s",
                      indent.c_str(),it->second.pid,it->second.ppid,it->second.state,it->second.mem_usage,it->second.cpu_usage,
                      it->second.io_read_rate,it->second.io_write_rate,it->second.rchar/1024,it->second.wchar/1024,
                      it->second.shared_clean,it->second.private_dirty,it->second.fd_count,it->second.num_threads,
                      it->second.voluntary_ctxt_switches,it->second.process_age,it->second.priority,it->second.nice,
                      it->second.cpus_allowed_list.c_str(),it->second.net_rx_rate,it->second.net_tx_rate,cmd.c_str());
            wattroff(win,COLOR_PAIR(color));
            if(line==selected_row)
            {
                wattroff(win,A_REVERSE);
            }
        }
        line++;
        auto children=process_tree.find(pid);
        if(children!=process_tree.end())
        {
            for(const auto &child:children->second)
            {
                displayTree(child,depth+1,line,max_lines,max_cols);
            }
        }
    }

    void displayProcesses(int max_lines,int max_cols)
    {
        int line=0;
        for(const auto &proc:processes)
        {
            // Skip if we're only showing zombies/orphans and this process isn't one
            if(show_only_zombies_orphans && !(proc.state=='Z'||(proc.ppid==1&&proc.pid!=1)))
            {
                line++;
                continue;
            }

            if(line>=max_lines+scroll_offset||line<scroll_offset)
            {
                line++;
                continue;
            }
            std::string cmd=proc.cmd;
            if(cmd.length()>20)
            {
                cmd=cmd.substr(0,17)+"...";
            }
            int color=(proc.state=='Z')?3:(proc.ppid==1&&proc.pid!=1)?4:(proc.cpu_usage>50.0)?2:1;
            if(line==selected_row)
            {
                wattron(win,A_REVERSE);
            }
            wattron(win,COLOR_PAIR(color));
            mvwprintw(win,line-scroll_offset+3,0,"%-5d %-5d %c %6.2f %6.2f %6.2f %6.2f %6llu %6llu %6llu %6llu %4llu %4ld %6llu %5.2f %3ld %3ld %-6s %6.2f %6.2f %-20s",
                      proc.pid,proc.ppid,proc.state,proc.mem_usage,proc.cpu_usage,
                      proc.io_read_rate,proc.io_write_rate,proc.rchar/1024,proc.wchar/1024,
                      proc.shared_clean,proc.private_dirty,proc.fd_count,proc.num_threads,
                      proc.voluntary_ctxt_switches,proc.process_age,proc.priority,proc.nice,
                      proc.cpus_allowed_list.c_str(),proc.net_rx_rate,proc.net_tx_rate,cmd.c_str());
            wattroff(win,COLOR_PAIR(color));
            if(line==selected_row)
            {
                wattroff(win,A_REVERSE);
            }
            line++;
        }
    }

    void handleInput(int ch)
    {
        int max_lines=getmaxy(win)-3;
        int total_lines=processes.empty()?0:processes.size();
        status_msg.clear();
        switch(ch)
        {
            case KEY_UP:
            {
                if(selected_row>0)
                {
                    selected_row--;
                }
                if(selected_row<scroll_offset)
                {
                    scroll_offset--;
                }
                needs_redraw=true;
                break;
            }
            case KEY_DOWN:
            {
                if(selected_row<total_lines-1)
                {
                    selected_row++;
                }
                if(selected_row>=scroll_offset+max_lines)
                {
                    scroll_offset++;
                }
                needs_redraw=true;
                break;
            }
            case KEY_PPAGE:
            {
                selected_row-=max_lines;
                scroll_offset-=max_lines;
                if(selected_row<0)
                {
                    selected_row=0;
                }
                if(scroll_offset<0)
                {
                    scroll_offset=0;
                }
                needs_redraw=true;
                break;
            }
            case KEY_NPAGE:
            {
                if(selected_row+max_lines<total_lines)
                {
                    selected_row+=max_lines;
                }
                else
                {
                    selected_row=total_lines-1;
                }
                if(selected_row>=scroll_offset+max_lines)
                {
                    scroll_offset=selected_row-max_lines+1;
                }
                needs_redraw=true;
                break;
            }
            case KEY_HOME:
            {
                selected_row=0;
                scroll_offset=0;
                needs_redraw=true;
                break;
            }
            case KEY_END:
            {
                selected_row=total_lines-1;
                if(selected_row>=max_lines)
                {
                    scroll_offset=selected_row-max_lines+1;
                }
                else
                {
                    scroll_offset=0;
                }
                needs_redraw=true;
                break;
            }
            case KEY_F(4):
            {
                char input[256]={0};
                echo();
                mvwprintw(win,0,0,"Filter (e.g., pid:1234 cpu>50): ");
                wrefresh(win);
                wgetnstr(win,input,255);
                noecho();
                std::string s=input;
                if(s.empty())
                {
                    filters.clear();
                    scanProcesses();
                    status_msg="Filter cleared";
                }
                else
                {
                    filters=parseFilters(s);
                    if(filters.empty())
                    {
                        status_msg=status_msg.empty()?"Invalid filter format":status_msg;
                    }
                    else
                    {
                        processes=filterProcesses();
                        status_msg="Filter applied: "+s;
                    }
                }
                selected_row=0;
                scroll_offset=0;
                needs_redraw=true;
                break;
            }
            case KEY_F(5):
            {
                tree_view=!tree_view;
                selected_row=0;
                scroll_offset=0;
                status_msg=tree_view?"Tree view enabled":"List view enabled";
                needs_redraw=true;
                break;
            }
            case KEY_F(6):
            {
                sort_criterion=(sort_criterion=="cpu")?"mem":(sort_criterion=="mem")?"io":(sort_criterion=="io")?"net":"cpu";
                sortProcesses(sort_criterion);
                selected_row=0;
                scroll_offset=0;
                status_msg="Sort by: "+sort_criterion;
                needs_redraw=true;
                break;
            }
            case KEY_F(9):
            {
                if(selected_row>=0&&selected_row<processes.size())
                {
                    auto proc=processes[selected_row];
                    std::string prompt;
                    bool kill_parent=false;
                    if(proc.state=='Z')
                    {
                        prompt="Zombie PID="+std::to_string(proc.pid)+" Cmd="+proc.cmd+" Kill parent PPID="+std::to_string(proc.ppid)+"? (y/n): ";
                        kill_parent=true;
                    }
                    else if(proc.ppid==1&&proc.pid!=1)
                    {
                        prompt="Orphan PID="+std::to_string(proc.pid)+" Cmd="+proc.cmd+" Kill? (y/n): ";
                    }
                    else
                    {
                        prompt="Kill PID="+std::to_string(proc.pid)+" Cmd="+proc.cmd+"? (y/n): ";
                    }
                    mvwprintw(win,0,0,"%s",prompt.c_str());
                    wrefresh(win);
                    char c=getch();
                    if(c=='y'||c=='Y')
                    {
                        pid_t pid=kill_parent?proc.ppid:proc.pid;
                        if(kill(pid,SIGTERM)==0)
                        {
                            status_msg="SIGTERM sent to PID "+std::to_string(pid);
                        }
                        else
                        {
                            status_msg="Error: Failed to send SIGTERM to PID "+std::to_string(pid);
                        }
                        scanProcesses();
                        if(!filters.empty())
                        {
                            processes=filterProcesses();
                        }
                        if(!sort_criterion.empty())
                        {
                            sortProcesses(sort_criterion);
                        }
                        selected_row=std::min(selected_row,static_cast<int>(processes.size())-1);
                        if(selected_row<0)
                        {
                            selected_row=0;
                        }
                        scroll_offset=std::min(scroll_offset,std::max(0,static_cast<int>(processes.size())-max_lines));
                    }
                    else
                    {
                        status_msg="Kill cancelled";
                    }
                }
                else
                {
                    status_msg="No process selected";
                }
                needs_redraw=true;
                break;
            }
            case 'z':
            case 'Z':
            {
                show_only_zombies_orphans=!show_only_zombies_orphans;
                status_msg=show_only_zombies_orphans?"Showing only zombies and orphans":"Showing all processes";
                selected_row=0;
                scroll_offset=0;
                needs_redraw=true;
                break;
            }
            case 'l':
            case 'L':
            {
                logging_enabled=!logging_enabled;
                if(!logging_enabled&&log_file.is_open())
                {
                    log_file.close();
                }
                status_msg=logging_enabled?"Logging enabled":"Logging disabled";
                needs_redraw=true;
                break;
            }
            case 'q':
            case 'Q':
            {
                endwin();
                if(log_file.is_open())
                {
                    log_file.close();
                }
                exit(0);
            }
            case KEY_RESIZE:
            {
                endwin();
                refresh();
                werase(win);
                needs_redraw=true;
                break;
            }
        }
    }

public:
    ProcessAnalyzer()
    {
        clk_tck=sysconf(_SC_CLK_TCK);
        std::ifstream uptime_file("/proc/uptime");
        if(uptime_file)
        {
            uptime_file>>system_uptime;
            uptime_file.close();
        }
        initscr();
        start_color();
        init_pair(1,COLOR_GREEN,COLOR_BLACK);
        init_pair(2,COLOR_RED,COLOR_BLACK);
        init_pair(3,COLOR_YELLOW,COLOR_BLACK);
        init_pair(4,COLOR_BLUE,COLOR_BLACK);
        cbreak();
        noecho();
        keypad(stdscr,TRUE);
        curs_set(0);
        timeout(50);
        win=newwin(0,0,0,0);
    }

    ~ProcessAnalyzer()
    {
        if(log_file.is_open())
        {
            log_file.close();
        }
        endwin();
    }

    void run()
    {
        while(true)
        {
            scanProcesses();

            // Check for zombies and orphans and prompt user to kill them
            if(promptKillZombiesOrphans())
            {
                // If processes were killed, rescan to update the process list
                scanProcesses();
            }

            if(!filters.empty())
            {
                processes=filterProcesses();
            }
            if(!sort_criterion.empty())
            {
                sortProcesses(sort_criterion);
            }
            if(logging_enabled)
            {
                logProcesses();
            }
            if(needs_redraw)
            {
                werase(win);
                displayHeader();
                mvwprintw(win,3,0,"%-5s %-5s %s %6s %6s %6s %6s %6s %6s %6s %6s %4s %4s %6s %5s %3s %3s %-6s %6s %6s %-20s",
                          "PID","PPID","S","Mem%","CPU%","IO R","IO W","RChar","WChar","Shared","Priv","FD","Thrd","CtxtSw","Age","Pri","Nice","CPUs","Net R","Net W","Cmd");
            }
            int max_lines=getmaxy(win)-4;
            int max_cols=getmaxx(win);
            if(processes.empty())
            {
                if(needs_redraw)
                {
                    mvwprintw(win,4,0,"No processes to display");
                }
            }
            else if(tree_view)
            {
                int line=0;
                displayTree(1,0,line,max_lines,max_cols);
            }
            else
            {
                displayProcesses(max_lines,max_cols);
            }
            wrefresh(win);
            needs_redraw=false;
            auto start=std::chrono::steady_clock::now();
            while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count()<poll_interval)
            {
                int ch=getch();
                if(ch!=ERR)
                {
                    handleInput(ch);
                    if(needs_redraw)
                    {
                        werase(win);
                        displayHeader();
                        mvwprintw(win,3,0,"%-5s %-5s %s %6s %6s %6s %6s %6s %6s %6s %6s %4s %4s %6s %5s %3s %3s %-6s %6s %6s %-20s",
                                  "PID","PPID","S","Mem%","CPU%","IO R","IO W","RChar","WChar","Shared","Priv","FD","Thrd","CtxtSw","Age","Pri","Nice","CPUs","Net R","Net W","Cmd");
                    }
                    if(processes.empty())
                    {
                        if(needs_redraw)
                        {
                            mvwprintw(win,4,0,"No processes to display");
                        }
                    }
                    else if(tree_view)
                    {
                        int line=0;
                        displayTree(1,0,line,max_lines,max_cols);
                    }
                    else
                    {
                        displayProcesses(max_lines,max_cols);
                    }
                    wrefresh(win);
                    needs_redraw=false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
};

int main()
{
    ProcessAnalyzer analyzer;
    analyzer.run();
    return 0;
}