# Monitoring system implementation based on Modular Stack

## 1. Goal and requirements

The goal of the monitoring system is to provide experts and shift crew in the ALICE Control Centre with an in-depth state of the O<sup>2</sup> computing farm. The near-real-time and historical graphical dashboards interface with users providing outstanding experience and allow grasp easily large amount of monitoring data.
In order to supply data to the end user the set of tools has been selected in the official evaluation to meet the requirements specified in the [O<sup>2</sup> Technical Design Report](https://cds.cern.ch/record/2011297/files/ALICE-TDR-019.pdf) and defined by O2 Work Package 8:
- Compatible with the O<sup>2</sup> reference operating system (currently CERN CentOS 7).
- Well documented.
- Actively maintained and supported by developers.
- Run in isolation when external services and/or connection to outside of ALICE are not available.
- Capable of handling 600 kHz input metric rate.
- Scalable to >> 600 kHz if necessary.
- Handle at least 100 000 sources.
- Introduce latency no higher than 500 ms up to the processing layer, and 1000 ms to the visualisation layer.
- Impose low storage size per measurement.
- Aligned with functional architecture:
  - Sensors.
  - Metric processing.
  - Historical record and near-real-time visualisation.
  - Alarming.
  - Storage that supports down-sampling, large input metric rates and low storage size.

  In addition, some optional requirements may positively influence the final rating:
  -	Supported by CERN or used in one of the experiments/departments.
  -	Self-recovery in case of connectivity issues.

## 2. Functional architecture

Figure 1 shows the functional architecture of the system. Three monitoring data sources has been identified:
- Application (O<sup>2</sup> related)
- Process
- System.

![](images/gen_arch.png)

<p align="center">Figure 1. Monitoring architecture</p>

These data sources send the monitoring data periodically to the server-side processing and aggregation task which can perform stream or batch operations such as:
- Suppression
- Enrichment
- Correlation
- Custom aggregation and others.

Afterwards data are forwarded to both storage and real-time dashboard. The selected storage must be support high input metric rate, low storage size and downsampling. The near-real-time dashboard receives selected, processed metrics as soon as it is possible in order to allow experts to react to abnormal situations. This imposes a need of low latency transport layer.
The historical dashboard displays data from the storage. As it has access to larger variety of metrics it is mostly used by experts in order to drill down the issues and access detailed views.
These dashboards display data as time series plots, gauges, bar, and other graphical objects. They allow access from various operating systems and from outside of the experimental area (Point 2).
Eventually, the alarming component detects abnormal situations and notifies experts in form on text message or other mean of notification.

## 3. The Modular Stack
The Modular Stack solution aims at fulfilling the requirements specified in the Section 1 by using multiple, replaceable tools. Such approach enables the possibility of replacing one or more of the selected components in case alternative options provide improved performance or additional functionalities. Moreover, opting for open source tools supported by developers ensures reliability, performance improvement. The provided documentation took was a important feature during the tool selection. For all the softwares external resources like website documentations, mailing lists (Google Groups), Github, books and tutorial are available to accelerating the learning curve and simplifying the project handover. Moreover, all the selected tools are compatible with the most important Linux distributions, including CERN CentOS 7.
The Modular Stack requires maintaining multiple tools and therefore compatibility between them. This results in higher system complexity and necessity to acquire knowledge on all the components. In case one of the selected tools breaks backward compatibility, becomes obsolete or its maintenance or support is dropped, the system might need to be adjusted or even redesigned. On the other hand, only standardised protocols are used for the communication which can facilitate any future migration. There is also the possibility that newly introduced features will require the purchasing of a subscription or license.
Following an overview of the chosen tools. More details of each components will be described in the next sections.
The monitoring system collects three different types of monitoring data: application, process and system information. The first two are covered from a O2 monitoring library, whereas the third is provided using an external tool. [CollectD](http://collectd.org/) was selected for retrieving system metrics (related to CPU, memory and I/O) from all hosts belonging to the O2 Facility. The high monitoring data rate requires a transport layer capable to manage and route all collected data. [Apache Flume](https://flume.apache.org/), "a distributed and highly-reliable service for collecting, aggregating and moving large amounts of data in a very efficient way. ", has been selected. Moreover, it could execute also simple processing tasks. As storing component [InfluxDB](https://docs.influxdata.com/influxdb/v1.5/), "a custom high-performance data store written specifically for time series data", fulfils all the above requirements. [Grafana](https://grafana.com/) is selected as graphical interface to display time series data for near-real-time and historical prospectives. [Riemann](http://riemann.io/) provides useful ways to forward externally alarms using multiple plugins and allows to implement some processing tasks internally. All remaining more complex processing tasks are implemented through [Apache Spark](https://spark.apache.org/), "a fast and general-purpose engine for large-scale data processing". The figure 2 shows the actual architecture of Modular Stack with all the components enunciated.

![](images/monsta_arc.png)

<p align="center">Figure 2. Modular Stack architecture</p>     

### 2.1 Sensors
The O2 Monitoring subsystem collects three classes of metrics:
- Application.
- Process.
- System (and infrastructure).
All these metrics are pushed to the backend for the processing, aggregation and storage.

#### 2.1.1 Collectd - system metrics
| Plugin    | Metric description | Requirements | Comments |
| --------- | ------------------ | ------------ | -------- |
| CPU       | Amount of time spent by the CPU in various states (user, system, io, idle, irq) | `/proc` | Jiffie unit |
| Interface | Throughput, packets/second and errors/second per interface | `/proc` | List of interfaces can be defined |
| Memory    | Memory utilisation (Used, Buffered, Cached, Free) | `/proc` | - |
| DF        | Used and available disk space per mounted partition | `statfs`, `getmntent` | List of partitions can be defined |
| Load      | System load as 1, 5, 15 minutes average | `/proc` | - |
| Uptime    | Execution time; current, average and maximum | `/proc` | - |
| Disk      | Disk performance metrics per read and write: average time, operation per second, io time and more | `/proc/diskstats` | - |
| Logfile   | Internal; writes collectd logs into a file | - | - |

Data from collectd can be transferred by one of two plugins (the selection is done in the Ansible recipe)
 - Network - binary protocol over UDP, natively supported by InfluxDB
 - Write HTTP - Formats data JSON and send over HTTP, Supported by Flume JSON Collectd HTTP Handler

#### 2.1.2 Process related metrics
| Metric    | Metric description | Requirements |
| --------- | ------------------ | ------------ |
| Bytes received and transmitted | Number of bytes received and transmitted by the process (per interface) | `/proc` |
| pmem | CPU usage | `ps` |
| pcpu | Memory usage | `ps` |

#### 2.1.3 Application specific metrics
The Application metric collection provides an entry point from O2 processes to the Monitoring subsystem. It forwards user defined metrics to the processing backend via connection or connection-less transport protocols.

### 2.2 Transport Layer
The large amount of monitoring data generated from the O2 Facility requires a high-performance transport layer. The main goal of this component is to receive monitoring data from the sensors installed on every host in the O2 Facility and route them towards the historical storage(InfluxDB), near-real-time dashboard(Grafana), Alarming component(Riemann) and computing unit(Apache Spark), as shown in Figure 2. Routing capability allows to send incoming data from the same data source towards different consumers. Apache Flume has been selected to fulfill the transport layer requirements. It is defined a "distributed, reliable, and available system for efficiently collecting, aggregating and moving large amounts of log data from many different sources to a centralized data store". Moreover, it "can be used to transport massive quantities of event data including but not limited to network traffic data, social-media-generated data, email messages and pretty much any data source possible". The Flume data flow model depends on:

-	the Flume event, defined "as a unit of data flow having a byte payload and an optional set of string attributes".
-	the Flume agent, a "process that hosts the components through which events flow from an external source to the next destination (hop)".

![](images/flume-arc.png)
<p align="center">Figure 3. Flume agent</p>

The external source sends events to Flume in a format that is recognised by the target Flume source. When a Flume source receives an event, it stores it into one or more channels. The channel is a passive store that keeps the event until it's consumed by a Flume sink. Channels could be store events in memory (for fast transmissions) or on local file system (for reliable transmissions). The sink removes the event from the channel and puts it into an external repository, like HDFS, or forwards it to the Flume source of the next Flume agent (next hop) in the flow. The source and sink within the given agent run asynchronously with the events staged in the channel.
An advantage to use Apache Flume is its high compatibility with other components belonging to the Hadoop ecosystem, like Apache Spark, providing natively components to receive and transmit data from and to the most important Hadoop components (HDFS, Apache HBase, Apache Hive, Apache Kafka, …. ). Unfortunately, Flume components able to read from and write to the selected tools are not provided. Custom components are been developed except for Apache Spark integration.
Beyond these three components, Flume provide also:

-	Interceptor: component ables to modify/drop event in-flight.
-	Channel selector: component ables to define to which channel forward the Flume event, depending on an attribute value.

These two components are related to a specific source and act after that and before the event is added to a channel.
Flume has some system requirements:

- Java Runtime Environment (Java 1.8 or later)
- Memory - Sufficient memory for configurations used by sources, channels or sinks
- Disk Space - Sufficient disk space for configurations used by channels or sinks
- Directory Permissions - Read/Write permissions for directories used by agent

As shown in the Fig 2 the Flume agents receive data from Application and Process Sensors and CollectD and transmit events to InfluxDB, Grafana, Riemann and Apache Spark. One of requirements is the capability to manage a large amount of monitoring data as quickly as possible. The memory channels have been selected to fulfill this requirement, since throughput is preferred to reliability.
The Flume components need a custom implementation are:

- InfluxDB Sink
- Grafana Sink
- Riemann Sink
- Application and Process information Source
- Collectd Source
- Enrichment Interceptor

The Fig 4 shows with more details the inner architecture of Flume components.

![](images/flume_inner_arc.png)
<p align="center">Figure 4. Flume inner architecture</p>

The channel selectors allow the routing of metric towards specific sinks using an attribute value contained in the Flume event: a switch-case structure route the event towards one or multiple channels. Generally, not all the metrics belonging to a group (application, process or collectd information) will be sent all towards a specific channel but a subgroup of them. To manage this need, interceptors could be inserted after the source in order to add specific attribute values to specific event in order to allow a successful routing. There is no interceptor after the Spark Source, since the information enrichment could be done directly in the Spark tasks. Following will be discuss of every of component shown in the Figure 4.

#### 2.2.1 InfluxDB Sink

The InfluxDB Sink allows to send data using HTTP or TCP packets. Tests shown the TCP protocol provides better performances respect low latency and throughput. So a InfluxDB UDP Sink has been developed. Since Apache Flume uses a Java Virtual Machine, the code can be written both in Java or Scala. For Flume custom components the Java programming language has been selected. The InfluxDB UDP Sink developed takes from the channel the Flume event, convert it to the [InfluxDB Line Protocol](https://docs.influxdata.com/influxdb/v1.5/write_protocols/line_protocol_reference/) and finally send the UDP packet to the hostname and port defined in the Flume agent configuration file.
The InfluxDB Line Protocol is a string containing:

-	Metric name.
-	Tags.
-	Values.
-	Timestamp (ns).

The tags and timestamp fields are optionals. The generic format is following shown:

```
"metric_name,tag1=key1,tag2=key2 value_name1=value1,value_name2=value2 timestamp_ns"
```

Value could have 4 types:

-	Long type is represented with a "i"-ending. Example: `"number_process count=10i"`.
-	Double type is represented normally. Example: `"perc_usage idle=0.98"`.
-	String type is represented inserting ` " `. Example: `"generic_metric hostname=\"test-machine\""`.
-	Boolean type is presented with the key word `true` or `false`. Example: `"generic_metric ping=true"`.

In order to keep the Flume event as more generic as possible, the metric name, tags, values and timestamp information are put directly in the event header. The event body is left empty.
Key-words have been given to these fields:

-	`name` for the metric_name
-	`timestamp` for timestamp
-	`tag_` as prefix of tags. Example `tag_host` and `tag_type_instance`.
-	`value_` as prefix of values. Example: `value_idle` and `value_hostid`.

If other key-words are found from the sink, they won't be considered.
For Example, the Flume event:

```JSON
[{"headers" : {
    "timestamp" : "434324343",
    "tag_host" : "cnaf0.infn.it",
    "tag_cpu" : "1",
    "tag_site" : "CNAF",
    "name" : "cpu",
    "value_idle" : "0.93",
    "value_user" : "0.03"
    },
  "body" : ""
  }
]
```

produces the InfluxDB Line Protocol:

```
"cpu,host=cnaf0.infn.it,site=CNAF,cpu=1 idle=0.93,user=0.03 434324343"
```

Further information are provided on the [Github page](https://github.com/AliceO2Group/MonitoringCustomComponents/tree/master/flume-udp-influxdb-sink)


#### 2.2.2 Grafana Sink

The NRT Grafana Sink has the goal to send data directly to the near-real-time Dashboard. Further details are not present since the component has not been developed yet.

#### 2.2.3 Riemann Sink

The Riemann Sink has the goal to send data to the Riemann instance in order to forwards message to expert.
Riemann accepts incoming data both in HTTP or TCP. Test or considerations will evaluate the best protocol to use.
Further details are not present since the component has not been developed yet.

#### 2.2.4 Spark Sink

The Spark Sink has the goal to send data to Spark in order to allow the execution of processing tasks. According the [Spark Streaming - Flume integration](https://spark.apache.org/docs/2.2.0/streaming-flume-integration.html) page there are two approaches to send data: Push-based approach and Pull-base approach.
In the first approach, Spark Streaming acts like a Flume Avro Source, thus the Spark Sink is essentially an Avro Sink. The second one, the Spark Sink acts like a buffer while "Spark Streaming uses a reliable Flume receiver and transactions to pull data from the sink". Actually the first approach is used, further details will be present in the Spark section. In this scenario the Spark Sink is implemented with a native Flume Avro Sink.

#### 2.2.5 Application and Process Source

The Application and Process Source has the goal to receive the data sent from the Application and Process sensors. Since the sensors are a custom implementations both HTTP and TCP approaches are available. Tests showed TCP protocol has better performance. In order to have an unique and general event format for all the Flume components, each collected packet is converted one o multiple events, where all metric fields are inserted into the headers Flume event fields. The UDP Flume Source is able to decode multiple events within UDP packet containing a JSON string type data like following:

```JSON
[{"headers" : {
    "timestamp" : "434324343",
    "tag_host" : "random_host.example.com",
    "name" : "cpu",
    "value_idle" : "0.13"
    },
  "body" : "random_body"
  }
]
```
The Body field is not decoded.
Further information to how configure the custom Flume UDP Source are provides on the [Github page](https://github.com/AliceO2Group/MonitoringCustomComponents/tree/master/flume-udp-source)

#### 2.2.6 CollectD Source

The CollectD Source has the goal to collect data coming from CollectD clients. HTTP protocol with [JSON format](https://collectd.org/wiki/index.php/Plugin:Write_HTTP) is selected for simplicity and consistency, since within the JSON is provided the value names, value formats and value fields. The "command format" even if used less bytes per metric, it requires an version dependent external file containing from which extract the metric name, type and format information. So to be version independent, the JSON format has been selected.
Flume natively provides a HTTP Source but it is not able to decode the CollectD JSON format. Custom handlers could be added to the HTTP Source to parse unsupported formats. The CollectD JSON HTTP Handler has been implemented instead of the whole custom HTTP Source.
Following an example of a CollectD JSON format is shown:

```JSON
[{ "values": [197141504, 175136768],
   "dstypes": ["counter", "counter"],
   "dsnames": ["read", "write"],
   "time": 1251533299.265,
   "interval": 10,
   "host": "leeloo.lan.home.verplant.org",
   "plugin": "disk",
   "plugin_instance": "sda",
   "type": "disk_octets",
   "type_instance": ""
   }
]
```

"Values", "dstypes" and "dsname" fields have a sorted list with the same number of elements. The "dstypes" fields defines the value type:

- COUNTER or ABSOLUTE is an unsigned integer.
- DERIVE is an signed integer.
- GAUGE is a double.

The following actions are implemented in the Collectd JSON HTTP Handler:

- Java `long` type has been used for unsigned and signed integer.
- The `time` field is stored in the `timestamp` Flume event field in nanoseconds and in long format.
- The `interval` field is not used.
- The `name` Flume event field is composed of `plugin` collectd JSON field and the the ith of `dsnames` field list e.g. "disk_read", "disk_write".
- The `host`, `plugin_instance`, `type`, `type_instance` fields are copied in the Flume event using the field prefix `tag_` e.g. "tag_host", "tag_plugin_instance".
- The `type_value` is added to the Flume event to insert the value type information and can assume only two values: "double" or "long" value.
- The value in put into the `value_value` Flume event.
- `plugin_instance`, `type`, `type_instance` empty fields are not inserted in the Flume event.

All data are in string format since stored in headers Flume Event field. The `type_value` field is considered optional.
Following the above actions, the CollectD JSON shown before produces the following Flume event.

```JSON
[{"headers" : {
    "timestamp" : "1251533299265000000",
    "tag_host" : "leeloo.lan.home.verplant.org",
    "name" : "disk_read",
    "value_value":  "197141504",
    "type_value" : "long",
    "tag_plugin_instance": "sda",
    "tag_type": "disk_octets"
    },
  "body" : ""
  },
 {"headers" : {
    "timestamp" : "1251533299265000000",
    "tag_host" : "leeloo.lan.home.verplant.org",
    "name" : "disk_write",
    "value_value" : "175136768",
    "type_value" : "long",
    "tag_plugin_instance": "sda",
    "tag_type": "disk_octets"
    },
  "body" : ""
  }
]
```

Further information about the component and how configure it in the Flume configuration file are described on the [Github page](https://github.com/AliceO2Group/MonitoringCustomComponents/tree/master/flume-json-collectd-http-handler).


#### 2.2.7 Spark Source
The Spark Source has the goal to receive the produced events from the Spark real-time processing tasks, so there is the freedom to select the best solution. Two approaches have been tested: using Avro events or UDP packets. They require an Avro Source and UDP Source, respectively. The implemented UDP Source could be used due its flexibility.

#### 2.2.8 Information Enrichment Interceptor
The information Enrichment Interceptor has the goal to add information to specific Flume events. Since the information is contained in the tags fields, this component adds specific tag-value couple to the event.

### 2.3 Storage
The storage component has the goal to store all the metric the Historical dashboard(s) need. High performances are required from this component:

- Well documented
- Actively maintained and supported by developers
- Run in isolation when external services and/or connection to outside of ALICE are not available
- Capable of handling high input metric rate
- Impose low storage size per measurement
- Downsampling
- Retention Policies

[InfluxDB](https://docs.influxdata.com/influxdb/v1.5/) is a "custom high-performance data store written specifically for time series data. It allows for high throughput ingest, compression and real-time querying of that same data". Its features make it the best solution to accomplish the database requirements.
InfluxDB allows to handle hundreds of data points per second: keeping the high precision raw data for only a limited time, and storing the lower precision. InfluxDB offers two features, Continuous Queries and Retention Policies, that help automate the process of downsampling data and expiring old data. InfluxDB provides InfluxQL as a SQL-like query language for interacting with stored data and low disk occupancy per measurement, three bytes for non-string values.

#### 2.3.1 Data organization
Timeseries data is stored in *measurements*, associable to the relation database tables. A database contains multiple measurements and multiple retention policies. Since a measurement could have the same name in multiple retention policies, an uniquely way to define it is: `<database_name>.<retention_policy_name>.<measurement_name>`. For example `collectd.ret_pol_1day.disk_read`

#### 2.3.2 Retention Policies and Continuous Queries
Adopt properly retention policies and continuous queries allow to minimize the disk usage and the computation requirements. Using retention policies, expire times can be associated to database. This feature provide a way to store both high time resolution data for a short period and low time resolution data for very long period. The downsampling processing could be done using continuous queries that read the source metric and compute the aggregation task to evaluate low time resolution time series. InfluxDB provides an extended set of aggregation function (mean, median, min, max, ...).

### 2.4 Dashboards
Historical and Near-real-time dashboards have the goal to plot time-series monitoring data using graphical objects on a interface accessible from the web.
[Grafana](https://grafana.com) has been chosen as data visualization tool since it offers customized historical record dashboards and real-time version is already foreseen in the road map. It can also generate alarms based on values coming from the database and thus not provide real-time alarms.
Grafana is able to retrieve data from multiple data sources like InfluxDB, Prometeus, ElasticSearch, Graphite and others, to provide an unique interface to visualize data coming from different back-ends.
Grafana supports multiple organizations in order to support a wide variety of deployment models, including using a single Grafana instance to provide service to multiple potentially untrusted Organizations.
Multiple organizations are configured to allow to multiple teams to share the same Grafana instance and work in an isolate environment.
A User is a named account in Grafana. A user can belong to one or more Organizations, and can be assigned different levels of privileges through roles.
A user’s organization membership is tied to a role that defines what the user is allowed to do in that organization. The roles are:
- Admin: add/edit data sources, add/edit organization users and teams, configure plugins and set organization settings.
- Editor: create/edit dashboards and alert rules
- Viewer: View any dashboard
All these roles will be configured to manage all the ALICE users.

Grafana supports a wide variety of internal and external ways for users to authenticate themselves and the CERN SSO it's used to allow CERN people to access to the dashboard, with a specific role.
A dashboard is logically divided in *rows* used to group *panels* together. Panel is the basic visualization building block. Currently five panel types are available:

- Graph
- Singlestat
- Dashlist
- Table
- Text

The Query Editor exposes capabilities of data source and allows to query metrics in a simple and graphical way. After having build a dashboard, the interface allows easily to setup the time window within plot the data and the refresh time.

The Historical dashboard has the goal to plot historical trends of significant metrics in order to valuate the functionality of all part in a wide time windows. The data to plot are downsampled historical data stored in the historical database (InfluxDB). For this purpose, the refresh time greater 10 seconds are considered adequate. Whereas the Near-real-time dashboard must plot data with the lowest latency, in any case below 1 second. To fulfill this requirement the data can't be retrieve so often from a database, a web-socket approach has been chosen. Grafana is planning to provide this feature in the next version.

### 2.5 Alarming
The alarming component has the goal to forward externally important alarms to experts. [Riemann](http://riemann.io/) has been selected since its capacity to inspect metrics on the fly and generate notifications when undesired behavior is detected. Riemann is able to execute simple processing on incoming data, like aggregation or filtering, and could support the streaming processing unit if needed. Its main task is the forwarding of alarms to experts. Riemann uses Clojure as programming language and in order to process events and send alerts and metrics a [Clojure](https://clojure.org/) code is needed. Clojure is a "dynamic, general-purpose programming language, combining the approachability and interactive development of a scripting language with an efficient and robust infrastructure for multithreaded programming". According the architecture shown in Figure 4, the Riemann instance receives data from a Flume custom Riemann Sink. HTTP, TCP and websocket protocol could be used to transmit events. Tests will established the best protocol. As alerts, email and Slack messages are been selected. Due to Riemann does not support cluster deployments, only simple and low cpu-usage processing tasks could be executed.

### 2.6 Batch and streaming processing
The selected processing applications are enrichment, aggregation, correlation and suppression tasks. Flume and Riemann could manage a subset of them - single event processing in Flume using interceptiors and simple aggregation and filtering processing in Riemann. The remaining tasks need a more powerful, horizontal scalable, complex processing unit. [Apache Spark](https://spark.apache.org/), "a fast and general-purpose engine for large-scale data processing" has been selected. Its key features are:

- *Ease to use*: it allows to write application quickly in Java, Scala, Python and R.
- *Speed*: it runs programs up to 100x faster the Hadoop MapReduce in memory.
- *Generality*: it allows to combine SQL, Streaming, Graph and complex analytics.

Spark is horizontal scalable and run on a cluster. Spark can run both by itself or over several existing cluster managers. Currently it supports these deployments:

- Standalone Deploy Mode
- Apache Mesos
- Hadoop YARN
- Kubernetes (still experimental)

Apache Mesos adds High Availability to Apache Spark, thanks its possibility to submit again a job if terminated with an error.
Spark revolves around the concept of a resilient distributed dataset (RDD), which is a fault-tolerant collection of elements that can be operated on in parallel.
Spark is able to execute both batch and streaming jobs. The selected elaboration tasks are performed using a streaming job, but with a similar code batch jobs could be executed on data stored in a long term archive. Spark provides the execution of streaming jobs by splitting the input data stream in batches of input data (RDD) which are processed using the batch functions. These batches of input data have a fixed time size. Spark allows to use [Map-Reduce](https://en.wikipedia.org/wiki/MapReduce) programming model making the processing of large input data rate with a parallel and distributed algorithms on a cluster. The Map functions fulfill the enrichment task, since acts event per event, whereas Reduce functions fulfill the aggregation task, since operate on all data belonging to the same RDD. Suppression and correlation tasks needed store old data in order to compute algorithms on them. Apache Spark allows to store data in memory, beyond in a no-volatile memory, like distributed storage. Since in this use case the processing the new data as quickly as possible is the priority, the in-memory processing is the best solution.
The Spark applications are written in Java and Scala, even if Scala is preferred since is less verbose and more intuitive.
The Spark Streaming-Flume integration could be done using [two approaches](https://spark.apache.org/docs/2.2.0/streaming-flume-integration.html):

- Flume-style Push-based approach
- Pull-based Approach using a Custom Sink

The first approach Spark Streaming is configured to act like a Flume Avro Source, so the Flume Sink can push data using a natively Avro Sink. The second approach uses a custom Sink in the Flume agent that buffers all incoming data and Spark Streaming uses a "reliable Flume receiver and transactions to pull data from the sink". This ensures stronger reliability and fault-tolerance guarantees than the previous approach. The second approach does not fit our use-case for to reasons:
- if the Spark job restarts it will begin from the data next the last retrieved, even if it's too old; the rule "the new data has more priority than old" is followed
- The time windows is shown to be not fixed in this approach

This reasons induced to use the push-based approach. So an Avro Sink is used in the Flume agent.

To implement the aggregation task, the `reduceByKeyAndWindow` function is used: it allows to aggregate Flume event received in the same time window and with the same `key` using a specific function. The *key* field depending on which variables is desired aggregate e.g. on time or on time-and-hosts.

The aggregated values are sent back to Flume in order to be consumed from the backends. Two approaches are been considered:
- send Avro events, so used an Avro Source as Spark Source
- send UDP packets and thus use the custom UDP Source as Spark Source

Both approaches have been tested successfully. The second one is preferred since it is a stateless and faster protocol.

## 3. Deployment

To allow quick deployment of the tools and ...
Ansible roles were prepared for the following components: [https://gitlab.cern.ch/AliceO2Group/system-configuration/tree/master/ansible]
 - Flume
 - Spark
 - collectd
 - InfluxDB